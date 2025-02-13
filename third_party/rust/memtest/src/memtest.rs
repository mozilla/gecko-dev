use {
    crate::{prelude::*, TimeoutChecker},
    rand::random,
    serde::{Deserialize, Deserializer, Serialize, Serializer},
    std::{error::Error, fmt},
};

// TODO: Intend to convert this module to a standalone `no_std` crate
// TODO: TimeoutChecker will be a trait instead

#[derive(Debug, Serialize, Deserialize)]
#[must_use]
pub enum MemtestOutcome {
    Pass,
    Fail(MemtestFailure),
}

#[derive(Debug, Serialize, Deserialize)]
pub enum MemtestFailure {
    /// Failure due to the actual value read being different from the expected value
    UnexpectedValue {
        address: usize,
        expected: usize,
        actual: usize,
    },
    /// Failure due to the two memory locations being compared returning two different values
    /// This is used by tests where memory is split in two and values are written in pairs
    MismatchedValues {
        address1: usize,
        value1: usize,
        address2: usize,
        value2: usize,
    },
}

#[derive(Debug, Serialize, Deserialize)]
pub enum MemtestError {
    Timeout,
    #[serde(
        serialize_with = "serialize_memtest_error_other",
        deserialize_with = "deserialize_memtest_error_other"
    )]
    Other(anyhow::Error),
}

macro_rules! memtest_kinds {{
    $($variant: ident),* $(,)?
} => {
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
    pub enum MemtestKind {
        $($variant,)*
    }

    impl MemtestKind {
        pub const ALL: &'static [Self] = &[
            $(Self::$variant),*
        ];
    }

    impl std::str::FromStr for MemtestKind {
        type Err = ParseMemtestKindError;
        fn from_str(s: &str) -> Result<Self, Self::Err> {
            match s {
            $(
                stringify!($variant) => Ok(Self::$variant),
            )*
                _ => Err(ParseMemtestKindError),
            }
        }
    }
}}

memtest_kinds! {
    OwnAddressBasic,
    OwnAddressRepeat,
    RandomVal,
    Xor,
    Sub,
    Mul,
    Div,
    Or,
    And,
    SeqInc,
    SolidBits,
    Checkerboard,
    BlockSeq,
}

#[derive(Debug, PartialEq, Eq)]
pub struct ParseMemtestKindError;

/// Write the address of each memory location to itself, then read back the value and check that it
/// matches the expected address.
#[tracing::instrument(skip_all)]
pub fn test_own_address_basic(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(2))
        .context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    for mem_ref in memory.iter_mut() {
        timeout_checker.check()?;
        write_volatile_safe(mem_ref, address_from_ref(mem_ref));
    }

    for mem_ref in memory.iter() {
        timeout_checker.check()?;
        let address = address_from_ref(mem_ref);
        let actual = read_volatile_safe(mem_ref);

        if actual != address {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected: address,
                actual,
            }));
        }
    }

    Ok(MemtestOutcome::Pass)
}

/// Write the address of each memory location (or its complement) to itself, then read back the
/// value and check that it matches the expected address.
/// This procedure is repeated 16 times.
#[tracing::instrument(skip_all)]
pub fn test_own_address_repeat(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    const NUM_RUNS: u64 = 16;
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(2 * NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    let val_to_write = |address: usize, i, j| {
        if (i + j) % 2 == 0 {
            address
        } else {
            !(address)
        }
    };

    for i in 0..usize::try_from(NUM_RUNS).unwrap() {
        for (j, mem_ref) in memory.iter_mut().enumerate() {
            timeout_checker.check()?;
            let val = val_to_write(address_from_ref(mem_ref), i, j);
            write_volatile_safe(mem_ref, val);
        }

        for (j, mem_ref) in memory.iter().enumerate() {
            timeout_checker.check()?;
            let address = address_from_ref(mem_ref);
            let expected = val_to_write(address, i, j);
            let actual = read_volatile_safe(mem_ref);

            if actual != expected {
                info!("Test failed at 0x{address:x}");
                return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                    address,
                    expected,
                    actual,
                }));
            }
        }
    }

    Ok(MemtestOutcome::Pass)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each
/// pair, write a random value. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_random_val(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        timeout_checker.check()?;
        let val = random();
        write_volatile_safe(first_ref, val);
        write_volatile_safe(second_ref, val);
    }

    compare_regions(first_half, second_half, &mut timeout_checker)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the XOR result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_xor(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, std::ops::BitXor::bitxor)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of subtracting a random value from
/// the value read from the location. After all locations are written, read and compare the two
/// halves.
#[tracing::instrument(skip_all)]
pub fn test_sub(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, usize::wrapping_sub)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of multiplying a random value with
/// the value read from the location. After all locations are written, read and compare the two
/// halves.
#[tracing::instrument(skip_all)]
pub fn test_mul(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, usize::wrapping_mul)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of dividing the value read from the
/// location with a random value. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_div(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, |n, d| {
        n.wrapping_div(usize::max(d, 1))
    })
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the OR result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_or(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, std::ops::BitOr::bitor)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the AND result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_and(
    memory: &mut [usize],
    timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    test_two_regions(memory, timeout_checker, std::ops::BitAnd::bitand)
}

/// Base function for `test_xor`, `test_sub`, `test_mul`, `test_div`, `test_or` and `test_and`
///
/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. Write to each pair using the given `write_val` function. After all
/// locations are written, read and compare the two halves.
fn test_two_regions(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
    transform_fn: fn(usize, usize) -> usize,
) -> Result<MemtestOutcome, MemtestError> {
    mem_reset(memory);
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        timeout_checker.check()?;

        let mixing_val = random();

        let val = read_volatile_safe(first_ref);
        let new_val = transform_fn(val, mixing_val);
        write_volatile_safe(first_ref, new_val);

        let val = read_volatile_safe(second_ref);
        let new_val = transform_fn(val, mixing_val);
        write_volatile_safe(second_ref, new_val);
    }

    compare_regions(first_half, second_half, &mut timeout_checker)
}

/// Split given memory into two halves and iterate through memory locations in pairs. Generate a
/// random value at the start. For each pair, write the result of adding the random value and the
/// index of iteration. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_seq_inc(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    let mut val: usize = random();
    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        timeout_checker.check()?;
        val = val.wrapping_add(1);
        write_volatile_safe(first_ref, val);
        write_volatile_safe(second_ref, val);
    }

    compare_regions(first_half, second_half, &mut timeout_checker)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each
/// pair, write to all bits as either 1s or 0s, alternating after each memory location pair.
/// After all locations are written, read and compare the two halves.
/// This procedure is repeated 64 times.
#[tracing::instrument(skip_all)]
pub fn test_solid_bits(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    const NUM_RUNS: u64 = 64;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    let mut solid_bits = !0;
    for _ in 0..NUM_RUNS {
        solid_bits = !solid_bits;
        let mut val = solid_bits;

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            timeout_checker.check()?;
            val = !val;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(failure) =
            compare_regions(first_half, second_half, &mut timeout_checker)?
        {
            return Ok(MemtestOutcome::Fail(failure));
        }
    }
    Ok(MemtestOutcome::Pass)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each pair,
/// write to a pattern of alternating 1s and 0s (in bytes it is either 0x55 or 0xaa, and alternating
/// after each memory location pair). After all locations are written, read and compare the two
/// halves.
/// This procedure is repeated 64 times.
#[tracing::instrument(skip_all)]
pub fn test_checkerboard(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    const NUM_RUNS: u64 = 64;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    let mut checker_board = usize_filled_from_byte(0xaa);

    for _ in 0..NUM_RUNS {
        checker_board = !checker_board;
        let mut val = checker_board;

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            timeout_checker.check()?;
            val = !val;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(failure) =
            compare_regions(first_half, second_half, &mut timeout_checker)?
        {
            return Ok(MemtestOutcome::Fail(failure));
        }
    }
    Ok(MemtestOutcome::Pass)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each pair,
/// write to all bytes with the value i. After all locations are written, read and compare the two
/// halves.
/// This procedure is repeated 256 times, with i corresponding to the iteration number 0-255.
#[tracing::instrument(skip_all)]
pub fn test_block_seq(
    memory: &mut [usize],
    mut timeout_checker: TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    const NUM_RUNS: u64 = 256;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    timeout_checker.init(expected_iter);

    for i in 0..=(u8::try_from(NUM_RUNS - 1).unwrap()) {
        let val = usize_filled_from_byte(i);

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            timeout_checker.check()?;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(failure) =
            compare_regions(first_half, second_half, &mut timeout_checker)?
        {
            return Ok(MemtestOutcome::Fail(failure));
        }
    }
    Ok(MemtestOutcome::Pass)
}

fn read_volatile_safe<T: Copy>(src: &T) -> T {
    unsafe { std::ptr::read_volatile(src) }
}

fn write_volatile_safe<T: Copy>(dst: &mut T, src: T) {
    unsafe { std::ptr::write_volatile(dst, src) }
}

fn split_slice_in_half(slice: &mut [usize]) -> anyhow::Result<(&mut [usize], &mut [usize])> {
    let mut it = slice.chunks_exact_mut(slice.len() / 2);
    let (Some(first), Some(second)) = (it.next(), it.next()) else {
        bail!("Insufficient memory length for two-regions memtest");
    };
    Ok((first, second))
}

fn mem_reset(memory: &mut [usize]) {
    for mem_ref in memory.iter_mut() {
        write_volatile_safe(mem_ref, !0);
    }
}

fn address_from_ref(r: &usize) -> usize {
    std::ptr::from_ref(r) as usize
}

/// Return a usize where all bytes are set to to value of `byte`
fn usize_filled_from_byte(byte: u8) -> usize {
    let mut val = 0;
    unsafe { std::ptr::write_bytes(&mut val, byte, 1) }
    val
}

fn compare_regions(
    region1: &mut [usize],
    region2: &mut [usize],
    timeout_checker: &mut TimeoutChecker,
) -> Result<MemtestOutcome, MemtestError> {
    for (ref1, ref2) in region1.iter().zip(region2.iter()) {
        timeout_checker.check()?;

        let address1 = address_from_ref(ref1);
        let address2 = address_from_ref(ref2);
        let val1 = read_volatile_safe(ref1);
        let val2 = read_volatile_safe(ref2);

        if val1 != val2 {
            info!("Test failed at 0x{address1:x} compared to 0x{address2:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::MismatchedValues {
                address1,
                value1: val1,
                address2,
                value2: val2,
            }));
        }
    }
    Ok(MemtestOutcome::Pass)
}

impl fmt::Display for ParseMemtestKindError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl Error for ParseMemtestKindError {}

impl fmt::Display for MemtestOutcome {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Outcome: {:?}", self)
    }
}

impl fmt::Display for MemtestError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Error: {:?}", self)
    }
}

impl Error for MemtestError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            MemtestError::Timeout => None,
            MemtestError::Other(err) => Some(err.as_ref()),
        }
    }
}

impl From<anyhow::Error> for MemtestError {
    fn from(err: anyhow::Error) -> MemtestError {
        MemtestError::Other(err)
    }
}

fn serialize_memtest_error_other<S>(error: &anyhow::Error, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    serializer.serialize_str(&format!("{:?}", error))
}

fn deserialize_memtest_error_other<'de, D>(deserializer: D) -> Result<anyhow::Error, D::Error>
where
    D: Deserializer<'de>,
{
    let str = String::deserialize(deserializer)?;
    Ok(anyhow!(str))
}
