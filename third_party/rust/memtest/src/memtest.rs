use {
    crate::prelude::*,
    rand::random,
    serde::{Deserialize, Deserializer, Serialize, Serializer},
    std::{error::Error, fmt},
};

// TODO: Intend to convert this module to a standalone `no_std` crate

pub type MemtestResult<O> = Result<MemtestOutcome, MemtestError<<O as TestObserver>::Error>>;

#[derive(Debug, Serialize, Deserialize)]
#[must_use]
pub enum MemtestOutcome {
    Pass,
    Fail(MemtestFailure),
}

#[derive(Serialize, Deserialize)]
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

pub trait TestObserver {
    type Error: Error;
    fn init(&mut self, expected_iter: u64);
    fn check(&mut self) -> Result<(), Self::Error>;
}

#[derive(Debug, Serialize, Deserialize)]
pub enum MemtestError<E> {
    Observer(E),
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
    MovInvFixedBlock,
    MovInvFixedBit,
    MovInvFixedRandom,
    MovInvWalk,
    BlockMove,
    MovInvRandom,
    Modulo20,
}

#[derive(Debug, PartialEq, Eq)]
pub struct ParseMemtestKindError;

impl MemtestKind {
    pub fn run<O: TestObserver>(&self, memory: &mut [usize], observer: O) -> MemtestResult<O> {
        let test = match self {
            Self::OwnAddressBasic => test_own_address_basic,
            Self::OwnAddressRepeat => test_own_address_repeat,
            Self::RandomVal => test_random_val,
            Self::Xor => test_xor,
            Self::Sub => test_sub,
            Self::Mul => test_mul,
            Self::Div => test_div,
            Self::Or => test_or,
            Self::And => test_and,
            Self::SeqInc => test_seq_inc,
            Self::SolidBits => test_solid_bits,
            Self::Checkerboard => test_checkerboard,
            Self::BlockSeq => test_block_seq,
            Self::MovInvFixedBlock => test_mov_inv_fixed_block,
            Self::MovInvFixedBit => test_mov_inv_fixed_bit,
            Self::MovInvFixedRandom => test_mov_inv_fixed_random,
            Self::MovInvWalk => test_mov_inv_walk,
            Self::BlockMove => test_block_move,
            Self::MovInvRandom => test_mov_inv_random,
            Self::Modulo20 => test_modulo_20,
        };
        test(memory, observer)
    }
}

/// Write the address of each memory location to itself, then read back the value and check that it
/// matches the expected address.
#[tracing::instrument(skip_all)]
pub fn test_own_address_basic<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(2))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        write_volatile_safe(mem_ref, address_from_ref(mem_ref));
    }

    for mem_ref in memory.iter() {
        observer.check().map_err(MemtestError::Observer)?;
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
pub fn test_own_address_repeat<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    const NUM_RUNS: u64 = 16;
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(2 * NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let val_to_write = |address: usize, i, j| {
        if (i + j) % 2 == 0 {
            address
        } else {
            !(address)
        }
    };

    for i in 0..usize::try_from(NUM_RUNS).unwrap() {
        for (j, mem_ref) in memory.iter_mut().enumerate() {
            observer.check().map_err(MemtestError::Observer)?;
            let val = val_to_write(address_from_ref(mem_ref), i, j);
            write_volatile_safe(mem_ref, val);
        }

        for (j, mem_ref) in memory.iter().enumerate() {
            observer.check().map_err(MemtestError::Observer)?;
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
pub fn test_random_val<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        observer.check().map_err(MemtestError::Observer)?;
        let val = random();
        write_volatile_safe(first_ref, val);
        write_volatile_safe(second_ref, val);
    }

    compare_regions(first_half, second_half, &mut observer)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the XOR result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_xor<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, std::ops::BitXor::bitxor)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of subtracting a random value from
/// the value read from the location. After all locations are written, read and compare the two
/// halves.
#[tracing::instrument(skip_all)]
pub fn test_sub<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, usize::wrapping_sub)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of multiplying a random value with
/// the value read from the location. After all locations are written, read and compare the two
/// halves.
#[tracing::instrument(skip_all)]
pub fn test_mul<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, usize::wrapping_mul)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the result of dividing the value read from the
/// location with a random value. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_div<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, |n, d| n.wrapping_div(usize::max(d, 1)))
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the OR result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_or<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, std::ops::BitOr::bitor)
}

/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. For each pair, write the AND result of a random value and the value
/// read from the location. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_and<O: TestObserver>(memory: &mut [usize], observer: O) -> MemtestResult<O> {
    test_two_regions(memory, observer, std::ops::BitAnd::bitand)
}

/// Base function for `test_xor`, `test_sub`, `test_mul`, `test_div`, `test_or` and `test_and`
///
/// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
/// memory locations in pairs. Write to each pair using the given `write_val` function. After all
/// locations are written, read and compare the two halves.
fn test_two_regions<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
    transform_fn: fn(usize, usize) -> usize,
) -> MemtestResult<O> {
    mem_reset(memory);
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        observer.check().map_err(MemtestError::Observer)?;

        let mixing_val = random();

        let val = read_volatile_safe(first_ref);
        let new_val = transform_fn(val, mixing_val);
        write_volatile_safe(first_ref, new_val);

        let val = read_volatile_safe(second_ref);
        let new_val = transform_fn(val, mixing_val);
        write_volatile_safe(second_ref, new_val);
    }

    compare_regions(first_half, second_half, &mut observer)
}

/// Split given memory into two halves and iterate through memory locations in pairs. Generate a
/// random value at the start. For each pair, write the result of adding the random value and the
/// index of iteration. After all locations are written, read and compare the two halves.
#[tracing::instrument(skip_all)]
pub fn test_seq_inc<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter =
        u64::try_from(first_half.len() * 2).context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let mut val: usize = random();
    for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
        observer.check().map_err(MemtestError::Observer)?;
        val = val.wrapping_add(1);
        write_volatile_safe(first_ref, val);
        write_volatile_safe(second_ref, val);
    }

    compare_regions(first_half, second_half, &mut observer)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each
/// pair, write to all bits as either 1s or 0s, alternating after each memory location pair.
/// After all locations are written, read and compare the two halves.
/// This procedure is repeated 64 times.
#[tracing::instrument(skip_all)]
pub fn test_solid_bits<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    const NUM_RUNS: u64 = 64;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let mut solid_bits = !0;
    for _ in 0..NUM_RUNS {
        solid_bits = !solid_bits;
        let mut val = solid_bits;

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            observer.check().map_err(MemtestError::Observer)?;
            val = !val;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(f) = compare_regions(first_half, second_half, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
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
pub fn test_checkerboard<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    const NUM_RUNS: u64 = 64;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let mut checker_board = usize_filled_from_byte(0xaa);

    for _ in 0..NUM_RUNS {
        checker_board = !checker_board;
        let mut val = checker_board;

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            observer.check().map_err(MemtestError::Observer)?;
            val = !val;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(f) = compare_regions(first_half, second_half, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
    }
    Ok(MemtestOutcome::Pass)
}

/// Split given memory into two halves and iterate through memory locations in pairs. For each pair,
/// write to all bytes with the value i. After all locations are written, read and compare the two
/// halves.
/// This procedure is repeated 256 times, with i corresponding to the iteration number 0-255.
#[tracing::instrument(skip_all)]
pub fn test_block_seq<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    const NUM_RUNS: u64 = 256;
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let expected_iter = u64::try_from(first_half.len() * 2)
        .ok()
        .and_then(|count| count.checked_mul(NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for i in 0..=(u8::try_from(NUM_RUNS - 1).unwrap()) {
        let val = usize_filled_from_byte(i);

        for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
            observer.check().map_err(MemtestError::Observer)?;
            write_volatile_safe(first_ref, val);
            write_volatile_safe(second_ref, val);
        }

        if let MemtestOutcome::Fail(f) = compare_regions(first_half, second_half, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
    }
    Ok(MemtestOutcome::Pass)
}

// Each call to the moving inversion algorithm iterates through the memory 3 times
const MOV_INV_ITERATIONS: u64 = 3;

/// This test adapts the moving inversion algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). As described in the the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms),
///
/// "The moving inversion tests work as follows:
/// 1. Fill memory with a pattern
/// 2. Starting at the lowest address
///     i. check that the pattern has not changed
///     ii. write the pattern's complement
///     iii. increment the address
///     iv. repeat 2.1 to 2.3
/// 3. Starting at the highest address
///     i. check that the pattern has not changed
///     ii. write the pattern's complement
///     iii. decrement the address
///     iv. repeat 3.1 to 3.3 "
///
/// This test runs the moving inversion algorithm with fixed patterns of all bits as 1s or 0s.
#[tracing::instrument(skip_all)]
pub fn test_mov_inv_fixed_block<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(MOV_INV_ITERATIONS * 2))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    if let MemtestOutcome::Fail(f) = mov_inv_fixed_pattern(memory, 0, &mut observer)? {
        return Ok(MemtestOutcome::Fail(f));
    }
    mov_inv_fixed_pattern(memory, !0, &mut observer)
}

/// This test adapts the moving inversion algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). For a detailed explanation of the algorithm, please refer
/// to the description available at the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms)
///
/// This test runs the moving inversion algorithm with fixed 8-bit patterns where 1 bit is 1/0 and the
/// other 7 bits are 0/1s.  The procedure is repeated 8 times with the pattern rotated by 1 bit each
/// time to test all bits in a byte.
#[tracing::instrument(skip_all)]
pub fn test_mov_inv_fixed_bit<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    const NUM_RUNS: u64 = 8;
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(MOV_INV_ITERATIONS * 2 * NUM_RUNS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let mut pattern = usize_filled_from_byte(0x10);
    for _ in 0..NUM_RUNS {
        if let MemtestOutcome::Fail(f) = mov_inv_fixed_pattern(memory, pattern, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
        if let MemtestOutcome::Fail(f) = mov_inv_fixed_pattern(memory, !pattern, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
        pattern = pattern.rotate_right(1);
    }
    Ok(MemtestOutcome::Pass)
}

/// This test adapts the moving inversion algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). For a detailed explanation of the algorithm, please refer
/// to the description available at the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms)
///
/// This test runs the moving inversion algorithm with a random fixed pattern.
#[tracing::instrument(skip_all)]
pub fn test_mov_inv_fixed_random<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(MOV_INV_ITERATIONS * 2))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let pattern = random();
    if let MemtestOutcome::Fail(f) = mov_inv_fixed_pattern(memory, pattern, &mut observer)? {
        return Ok(MemtestOutcome::Fail(f));
    }
    mov_inv_fixed_pattern(memory, !pattern, &mut observer)
}

fn mov_inv_fixed_pattern<O: TestObserver>(
    memory: &mut [usize],
    pattern: usize,
    observer: &mut O,
) -> MemtestResult<O> {
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        write_volatile_safe(mem_ref, pattern);
    }

    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        let address = address_from_ref(mem_ref);
        let actual = read_volatile_safe(mem_ref);

        if actual != pattern {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected: pattern,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, !pattern);
    }

    for mem_ref in memory.iter_mut().rev() {
        observer.check().map_err(MemtestError::Observer)?;
        let address = address_from_ref(mem_ref);
        let actual = read_volatile_safe(mem_ref);

        if actual != !pattern {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected: !pattern,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, pattern);
    }

    Ok(MemtestOutcome::Pass)
}

/// This test adapts the moving inversion algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). For a detailed explanation of the algorithm, please refer
/// to the description available at the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms)
///
/// This test runs the moving inversion algorithm with a "walking" bit pattern. The algorithm starts
/// with 0x1 (or the compliment of 0x1) and "walks" the bit by shifting left for every new memory
/// location.
/// The procedure is repeated with offsets 0-31 or 0-63 depending on the size of `usize` to test all
/// bits in a memory location.
#[tracing::instrument(skip_all)]
pub fn test_mov_inv_walk<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    const NUM_RUNS: u32 = usize::BITS;
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(MOV_INV_ITERATIONS * 2 * u64::from(NUM_RUNS)))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for i in 0..NUM_RUNS {
        let pattern = 1 << i;
        if let MemtestOutcome::Fail(f) = mov_inv_walking_pattern(memory, pattern, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
        if let MemtestOutcome::Fail(f) = mov_inv_walking_pattern(memory, !pattern, &mut observer)? {
            return Ok(MemtestOutcome::Fail(f));
        }
    }
    Ok(MemtestOutcome::Pass)
}

fn mov_inv_walking_pattern<O: TestObserver>(
    memory: &mut [usize],
    starting_pattern: usize,
    observer: &mut O,
) -> MemtestResult<O> {
    let mut pattern = starting_pattern;
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        write_volatile_safe(mem_ref, pattern);
        pattern = pattern.rotate_left(1);
    }

    pattern = starting_pattern;
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        let address = address_from_ref(mem_ref);
        let actual = read_volatile_safe(mem_ref);

        if actual != pattern {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected: pattern,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, !pattern);
        pattern = pattern.rotate_left(1);
    }

    pattern = !pattern;
    for mem_ref in memory.iter_mut().rev() {
        observer.check().map_err(MemtestError::Observer)?;
        pattern = pattern.rotate_right(1);
        let address = address_from_ref(mem_ref);
        let actual = read_volatile_safe(mem_ref);

        if actual != pattern {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected: pattern,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, !pattern);
    }
    Ok(MemtestOutcome::Pass)
}

/// This test adapts the block move test algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). For a detailed explanation of the algorithm, please refer to
/// the description available at the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms)
///
/// The test aims to stress test the memory by moving blocks of memory, such as with the `movs` instruction.
/// It first initializes the memory with an irregular shifting pattern. Then it performs 3 memory block moves.
/// 1. Copy the first half of the memory region to the second half
/// 2. Copy the second half of the memory region back to the first half, offset by 8 locations.
///    ie. Copy the second half - the last 8 locations, to the first half's original location + 8
/// 3. Copy the second half's last 8 locations to the first half's first 8 locations
/// Finally, the test verifies that the second half has the values of the original first half, and
/// the first half's values are right rotated by 8 locations
///
/// Note that the original implementation in Memtest86+ only verifies the values by comparing
/// neighbouring pairs of memory location. Instead of that, this implementation verifies by
/// recalculating the expected pattern in each location.
///
/// Also note that unlike the other memory tests, `Observer::check()` is only called in the initial
/// loop for initalizing memory and the final loop for verifying values. It is not called during the
/// memory block moves, as it intefere with the stress testing of memory. Unfortunately, this means
/// that `Observer::check()` will not be called for a siginficant duration of the test run time.
#[tracing::instrument(skip_all)]
pub fn test_block_move<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    const CHUNK_SIZE: usize = 16;
    const OFFSET: usize = 8;
    if memory.len() < CHUNK_SIZE {
        Err(anyhow!("Insufficient memory length for Block Move Test"))?;
    }

    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(2))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let val_to_write = |pattern: usize, idx| match idx {
        4 | 5 | 10 | 11 | 14 | 15 => !pattern,
        _ => pattern,
    };

    // Set up initial pattern in memory
    let mut pattern = 1;
    for chunk in memory.chunks_exact_mut(CHUNK_SIZE) {
        for (i, mem_ref) in chunk.iter_mut().enumerate() {
            observer.check().map_err(MemtestError::Observer)?;

            let val = val_to_write(pattern, i);
            write_volatile_safe(mem_ref, val);
        }
        pattern = pattern.rotate_left(1);
    }

    // Move blocks of memory around
    let (first_half, second_half) = split_slice_in_half(memory)?;
    let half_len = first_half.len();
    volatile_copy_slice(second_half, first_half);
    volatile_copy_slice(
        &mut first_half[OFFSET..],
        &second_half[..(half_len - OFFSET)],
    );
    volatile_copy_slice(
        &mut first_half[..OFFSET],
        &second_half[(half_len - OFFSET)..],
    );

    // Verify that values stored after block move are as expected, by traversing both halves at the
    // same time (with first half rotated by OFFSET), as they have the same expected values
    let mut pattern = 1;
    for (chunk1, chunk2) in [&first_half[OFFSET..], &first_half[..OFFSET]]
        .concat()
        .chunks(CHUNK_SIZE)
        .zip(second_half.chunks(CHUNK_SIZE))
    {
        for (i, (mem_ref1, mem_ref2)) in chunk1.iter().zip(chunk2.iter()).enumerate() {
            let expected = val_to_write(pattern, i);

            for mem_ref in [mem_ref1, mem_ref2] {
                observer.check().map_err(MemtestError::Observer)?;

                let address = address_from_ref(mem_ref);
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
        pattern = pattern.rotate_left(1);
    }

    Ok(MemtestOutcome::Pass)
}

// TODO: In Memtest86+, block move is achieved with the `movs` assembly instruction
fn volatile_copy_slice<T: Copy>(dst: &mut [T], src: &[T]) {
    assert_eq!(
        dst.len(),
        src.len(),
        "length of dst and src should be equal"
    );

    for (dst_ref, src_ref) in dst.iter_mut().zip(src.iter()) {
        let val = read_volatile_safe(src_ref);
        write_volatile_safe(dst_ref, val);
    }
}

/// This test adapts the moving inversion algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus). For a detailed explanation of the algorithm, please refer
/// to the description available at the [Memtest86+ Test Algorithm Section](https://github.com/
/// memtest86plus/memtest86plus?tab=readme-ov-file#memtest86-test-algorithms)
///
/// This test runs the moving inversion algorithm with a random pattern for every memory location.
#[tracing::instrument(skip_all)]
pub fn test_mov_inv_random<O: TestObserver>(
    memory: &mut [usize],
    mut observer: O,
) -> MemtestResult<O> {
    use {
        rand::{rngs::SmallRng, Rng, SeedableRng},
        std::time::{SystemTime, UNIX_EPOCH},
    };
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(MOV_INV_ITERATIONS))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let seed = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;

    let mut rng = SmallRng::seed_from_u64(seed);
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        write_volatile_safe(mem_ref, rng.gen());
    }

    let mut rng = SmallRng::seed_from_u64(seed);
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        let address = address_from_ref(mem_ref);
        let expected = rng.gen();
        let actual = read_volatile_safe(mem_ref);

        if actual != expected {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, !expected);
    }

    let mut rng = SmallRng::seed_from_u64(seed);
    for mem_ref in memory.iter_mut() {
        observer.check().map_err(MemtestError::Observer)?;
        let address = address_from_ref(mem_ref);
        let expected = !rng.gen::<usize>();
        let actual = read_volatile_safe(mem_ref);

        if actual != expected {
            info!("Test failed at 0x{address:x}");
            return Ok(MemtestOutcome::Fail(MemtestFailure::UnexpectedValue {
                address,
                expected,
                actual,
            }));
        }

        write_volatile_safe(mem_ref, !expected);
    }

    Ok(MemtestOutcome::Pass)
}

/// This test uses the Modulo-20 algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus),  which is designed to avoid effects of caching and buffering.
///
/// The test generates a random value, then write the value to every 20th memory location.
/// Afterwards write the complement of the value to all other locations one or more times (twice in
/// this case). Then verify that the values stored in every 20th location is unchanged.
///
/// The procedure is repeated with offsets 0-19 to test all memory locations.
#[tracing::instrument(skip_all)]
pub fn test_modulo_20<O: TestObserver>(memory: &mut [usize], mut observer: O) -> MemtestResult<O> {
    const STEP: usize = 20;
    (memory.len() > STEP)
        .then_some(())
        .context("Insufficient memory length for two-regions memtest")?;
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul((STEP * 2).try_into().unwrap()))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    let pattern = random();
    for offset in 0..STEP {
        for mem_ref in memory.iter_mut().skip(offset).step_by(STEP) {
            observer.check().map_err(MemtestError::Observer)?;
            write_volatile_safe(mem_ref, pattern);
        }

        for _ in 0..2 {
            for (i, mem_ref) in memory.iter_mut().enumerate() {
                if i % STEP == offset {
                    continue;
                }
                observer.check().map_err(MemtestError::Observer)?;
                write_volatile_safe(mem_ref, !pattern);
            }
        }

        for mem_ref in memory.iter().skip(offset).step_by(STEP) {
            observer.check().map_err(MemtestError::Observer)?;
            let address = address_from_ref(mem_ref);
            let expected = pattern;
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

fn compare_regions<O: TestObserver>(
    region1: &mut [usize],
    region2: &mut [usize],
    observer: &mut O,
) -> MemtestResult<O> {
    for (ref1, ref2) in region1.iter().zip(region2.iter()) {
        observer.check().map_err(MemtestError::Observer)?;

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

impl fmt::Debug for MemtestFailure {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::UnexpectedValue {
                address,
                expected,
                actual,
            } => f
                .debug_struct("UnexpectedValue")
                .field("address", &format_args!("0x{address:x}"))
                .field("expected", &format_args!("0x{expected:x}"))
                .field("actual", &format_args!("0x{actual:x}"))
                .finish(),
            Self::MismatchedValues {
                address1,
                value1,
                address2,
                value2,
            } => f
                .debug_struct("MismatchedValues")
                .field("address1", &format_args!("0x{address1:x}"))
                .field("value1", &format_args!("0x{value1:x}"))
                .field("address2", &format_args!("0x{address2:x}"))
                .field("value2", &format_args!("0x{value2:x}"))
                .finish(),
        }
    }
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

impl<E: fmt::Debug> fmt::Display for MemtestError<E> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Error: {:?}", self)
    }
}

impl<E: Error + 'static> Error for MemtestError<E> {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            MemtestError::Observer(err) => Some(err),
            MemtestError::Other(err) => Some(err.as_ref()),
        }
    }
}

impl<E> From<anyhow::Error> for MemtestError<E> {
    fn from(err: anyhow::Error) -> MemtestError<E> {
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

#[cfg(test)]
mod test {
    use {
        super::{MemtestKind, MemtestOutcome, TestObserver},
        std::convert::Infallible,
    };

    #[derive(Debug)]
    struct IterationCounter {
        expected_iter: Option<u64>,
        completed_iter: u64,
    }

    impl TestObserver for &mut IterationCounter {
        type Error = Infallible;

        fn init(&mut self, expected_iter: u64) {
            assert!(
                self.expected_iter.is_none(),
                "init() should only be called once per test"
            );

            self.expected_iter = Some(expected_iter);
        }

        #[inline(always)]
        fn check(&mut self) -> Result<(), Self::Error> {
            self.completed_iter += 1;
            Ok(())
        }
    }

    impl IterationCounter {
        fn new() -> IterationCounter {
            IterationCounter {
                expected_iter: None,
                completed_iter: 0,
            }
        }

        fn assert_count(self) {
            let expected_iter = self.expected_iter.expect("init() should be called");
            assert_eq!(expected_iter, self.completed_iter);
        }
    }

    #[test]
    fn test_memtest_expected_iter() {
        let mut memory = vec![0; 512];
        for test_kind in MemtestKind::ALL {
            let mut counter = IterationCounter::new();
            assert!(
                matches!(
                    test_kind.run(&mut memory, &mut counter),
                    Ok(MemtestOutcome::Pass),
                ),
                "Memtest should pass"
            );
            counter.assert_count();
        }
    }
}
