use {
    crate::prelude::*,
    rand::random,
    serde::{Deserialize, Deserializer, Serialize, Serializer},
    std::{error, fmt},
};

// TODO: Intend to convert this module to a standalone `no_std` crate

pub type TestResult<O> = Result<Outcome, Error<<O as TestObserver>::Error>>;

#[derive(Debug, Serialize, Deserialize)]
#[must_use]
pub enum Outcome {
    Pass,
    Fail(Failure),
}

#[derive(Serialize, Deserialize)]
pub enum Failure {
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
    type Error: error::Error;
    fn init(&mut self, expected_iter: u64);
    fn check(&mut self) -> Result<(), Self::Error>;
}

#[derive(Debug, Serialize, Deserialize)]
pub enum Error<E> {
    Observer(E),
    #[serde(
        serialize_with = "serialize_error_other",
        deserialize_with = "deserialize_error_other"
    )]
    Other(anyhow::Error),
}

macro_rules! test_kinds {{
    $($variant: ident),* $(,)?
} => {
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
    pub enum TestKind {
        $($variant,)*
    }

    impl TestKind {
        pub const ALL: &'static [Self] = &[
            $(Self::$variant),*
        ];
    }

    impl std::str::FromStr for TestKind {
        type Err = ParseTestKindError;
        fn from_str(s: &str) -> Result<Self, Self::Err> {
            match s {
            $(
                stringify!($variant) => Ok(Self::$variant),
            )*
                _ => Err(ParseTestKindError),
            }
        }
    }
}}

test_kinds! {
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
pub struct ParseTestKindError;

impl TestKind {
    pub fn run<O: TestObserver>(&self, memory: &mut [usize], observer: O) -> TestResult<O> {
        use two_region::{
            run_two_region_test_algorithm, And, BlockSeq, Checkerboard, Div, Mul, Or, RandomVal,
            SeqInc, SolidBits, Sub, Xor,
        };

        match self {
            Self::OwnAddressBasic => {
                run_test_algorithm(OwnAddressBasic::default(), memory, observer)
            }
            Self::OwnAddressRepeat => {
                run_test_algorithm(OwnAddressRepeat::default(), memory, observer)
            }
            Self::RandomVal => {
                run_two_region_test_algorithm(RandomVal::default(), memory, observer)
            }
            Self::Xor => run_two_region_test_algorithm(Xor::default(), memory, observer),
            Self::Sub => run_two_region_test_algorithm(Sub::default(), memory, observer),
            Self::Mul => run_two_region_test_algorithm(Mul::default(), memory, observer),
            Self::Div => run_two_region_test_algorithm(Div::default(), memory, observer),
            Self::Or => run_two_region_test_algorithm(Or::default(), memory, observer),
            Self::And => run_two_region_test_algorithm(And::default(), memory, observer),
            Self::SeqInc => run_two_region_test_algorithm(SeqInc::default(), memory, observer),
            Self::SolidBits => {
                run_two_region_test_algorithm(SolidBits::default(), memory, observer)
            }
            Self::Checkerboard => {
                run_two_region_test_algorithm(Checkerboard::default(), memory, observer)
            }
            Self::BlockSeq => run_two_region_test_algorithm(BlockSeq::default(), memory, observer),
            Self::MovInvFixedBlock => {
                run_test_algorithm(mov_inv::FixedBlock::default(), memory, observer)
            }
            Self::MovInvFixedBit => {
                run_test_algorithm(mov_inv::FixedBit::default(), memory, observer)
            }
            Self::MovInvFixedRandom => {
                run_test_algorithm(mov_inv::FixedRandom::default(), memory, observer)
            }
            Self::MovInvWalk => run_test_algorithm(mov_inv::Walk::default(), memory, observer),
            Self::BlockMove => run_block_move(memory, observer),
            Self::MovInvRandom => run_test_algorithm(mov_inv::Random::default(), memory, observer),
            Self::Modulo20 => run_modulo_20(memory, observer),
        }
    }
}

/// A pass can be done forward (default) or reversed
#[derive(Debug, Default)]
enum PassDirection {
    #[default]
    Forward,
    Reverse,
}

type PassFn<T> = fn(&mut T, direction: &mut PassDirection) -> IterFn<T>;
type IterFn<T> = fn(&mut T, &mut usize) -> Result<(), Failure>;

trait TestAlgorithm: fmt::Debug {
    /// The number of runs the algorithm needs. Most tests can just accept the default of '1'
    fn num_runs(&self) -> u64 {
        1
    }
    /// Initialize the state for the run given by `run_idx`. Most tests don't need to do anything.
    fn start_run(&mut self, _run_idx: u64) {}
    /// Return a list of functions defining the behavior of the passes
    ///
    /// Each function receives a mutable reference to `direction`, which will always be set to
    /// the default of `Forward`. If a pass wants the direction reversed, it can set it to
    /// `Reverse`.
    ///
    /// Each function must return the iteration function for the pass. This is the function that
    /// will be run for each memory address.
    ///
    /// The iteration function itself takes a mutable reference to each memory address and
    /// returns either `Ok(())` or `Err(Failure)`.
    fn passes(&self) -> Vec<PassFn<Self>>;
}

#[tracing::instrument(skip(memory, observer))]
fn run_test_algorithm<T: TestAlgorithm, O: TestObserver>(
    mut test: T,
    memory: &mut [usize],
    mut observer: O,
) -> TestResult<O> {
    let expected_iter = u64::try_from(memory.len())
        .ok()
        .and_then(|count| count.checked_mul(test.num_runs()))
        .and_then(|count| count.checked_mul(u64::try_from(test.passes().len()).ok()?))
        .context("Total number of iterations overflowed")?;
    observer.init(expected_iter);

    for i in 0..test.num_runs() {
        test.start_run(i);
        for pass_fn in test.passes() {
            let mut direction = PassDirection::default();
            let iter_fn = pass_fn(&mut test, &mut direction);

            let mem_iter: Box<dyn Iterator<Item = &mut usize>> = match direction {
                PassDirection::Forward => Box::new(memory.iter_mut()),
                PassDirection::Reverse => Box::new(memory.iter_mut().rev()),
            };

            for mem_ref in mem_iter {
                observer.check().map_err(Error::Observer)?;
                if let Err(f) = iter_fn(&mut test, mem_ref) {
                    return Ok(Outcome::Fail(f));
                }
            }
        }
    }
    Ok(Outcome::Pass)
}

fn check_expected_value(address: usize, expected: usize, actual: usize) -> Result<(), Failure> {
    if actual != expected {
        info!("Test failed at 0x{address:x}");
        Err(Failure::UnexpectedValue {
            address,
            expected,
            actual,
        })
    } else {
        Ok(())
    }
}

/// Write the address of each memory location to itself, then read back the value and check that it
/// matches the expected address.
#[derive(Debug, Default)]
struct OwnAddressBasic {}

impl TestAlgorithm for OwnAddressBasic {
    fn passes(&self) -> Vec<PassFn<Self>> {
        vec![Self::pass_0, Self::pass_1]
    }
}

impl OwnAddressBasic {
    fn pass_0(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
        |_state, mem_ref| {
            write_volatile_safe(mem_ref, address_from_ref(mem_ref));
            Ok(())
        }
    }

    fn pass_1(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
        |_state, mem_ref| {
            check_expected_value(
                address_from_ref(mem_ref),
                address_from_ref(mem_ref),
                read_volatile_safe(mem_ref),
            )
        }
    }
}

/// Write the address of each memory location (or its complement) to itself, then read back the
/// value and check that it matches the expected address.
/// This procedure is repeated 16 times.
#[derive(Debug, Default)]
struct OwnAddressRepeat {
    complement: bool,
    run_idx: u64,
}

impl TestAlgorithm for OwnAddressRepeat {
    fn num_runs(&self) -> u64 {
        16
    }
    fn start_run(&mut self, run_idx: u64) {
        self.run_idx = run_idx;
    }
    fn passes(&self) -> Vec<PassFn<Self>> {
        vec![Self::pass_0, Self::pass_1]
    }
}

impl OwnAddressRepeat {
    fn pass_0(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
        self.init_complement();

        |state, mem_ref| {
            write_volatile_safe(mem_ref, state.val_to_write(mem_ref));

            state.complement = !state.complement;
            Ok(())
        }
    }

    fn pass_1(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
        self.init_complement();

        |state, mem_ref| {
            check_expected_value(
                address_from_ref(mem_ref),
                state.val_to_write(mem_ref),
                read_volatile_safe(mem_ref),
            )?;

            state.complement = !state.complement;
            Ok(())
        }
    }

    fn init_complement(&mut self) {
        self.complement = self.run_idx % 2 != 0;
    }

    fn val_to_write(&self, mem_ref: &usize) -> usize {
        if self.complement {
            !address_from_ref(mem_ref)
        } else {
            address_from_ref(mem_ref)
        }
    }
}

mod two_region {
    use {
        super::{
            address_from_ref, mem_reset, read_volatile_safe, split_slice_in_half,
            usize_filled_from_byte, write_volatile_safe, Error, Failure, Outcome, TestObserver,
            TestResult,
        },
        crate::prelude::*,
        rand::random,
        std::fmt,
    };

    type TwoRegionWriteFn<T> = fn(&mut T, &mut usize, &mut usize);

    /// A two region test has to passes of memory for every run.  The test splits memory into
    /// two halves. The first pass iterates through memory and writes some memory pattern to each
    /// pair of locations. The second pass reads through and compare the two halves.
    trait TwoRegionTestAlgorithm: fmt::Debug + Default {
        /// The number of runs the algorithm needs. Most tests can just accept the default of '1'
        fn num_runs(&self) -> u64 {
            1
        }
        /// Initialize the state for the run given by `run_idx`. Most tests don't need to do anything.
        fn start_run(&mut self, _run_idx: u64) {}

        /// Returns whether a memory reset is needed at the start of each run
        /// If true, the algorithm will reset all bits to 1.
        fn reset_before_run(&self) -> bool;

        /// Returns the function that is called for every iteration of the first pass to write to
        /// memory.
        fn write_fn(&self) -> TwoRegionWriteFn<Self>;
    }

    #[tracing::instrument(skip(memory, observer))]
    pub(super) fn run_two_region_test_algorithm<T: TwoRegionTestAlgorithm, O: TestObserver>(
        mut test: T,
        memory: &mut [usize],
        mut observer: O,
    ) -> TestResult<O> {
        if test.reset_before_run() {
            mem_reset(memory);
        }
        let (first_half, second_half) = split_slice_in_half(memory)?;
        let expected_iter = u64::try_from(first_half.len())
            .ok()
            .and_then(|count| count.checked_mul(2))
            .and_then(|count| count.checked_mul(test.num_runs()))
            .context("Total number of iterations overflowed")?;
        observer.init(expected_iter);

        for i in 0..test.num_runs() {
            test.start_run(i);

            let write_fn = test.write_fn();
            for (first_ref, second_ref) in first_half.iter_mut().zip(second_half.iter_mut()) {
                observer.check().map_err(Error::Observer)?;
                write_fn(&mut test, first_ref, second_ref);
            }

            for (first_ref, second_ref) in first_half.iter().zip(second_half.iter()) {
                observer.check().map_err(Error::Observer)?;
                if let Err(f) = check_matching_values(
                    address_from_ref(first_ref),
                    read_volatile_safe(first_ref),
                    address_from_ref(second_ref),
                    read_volatile_safe(second_ref),
                ) {
                    return Ok(Outcome::Fail(f));
                }
            }
        }

        Ok(Outcome::Pass)
    }

    fn check_matching_values(
        address1: usize,
        value1: usize,
        address2: usize,
        value2: usize,
    ) -> Result<(), Failure> {
        if value1 != value2 {
            info!("Test failed at 0x{address1:x} compared to 0x{address2:x}");
            Err(Failure::MismatchedValues {
                address1,
                value1,
                address2,
                value2,
            })
        } else {
            Ok(())
        }
    }

    /// Split given memory into two halves and iterate through memory locations in pairs. For each
    /// pair, write a random value. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct RandomVal {}
    impl TwoRegionTestAlgorithm for RandomVal {
        fn reset_before_run(&self) -> bool {
            false
        }

        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            |_state, first_ref, second_ref| {
                let val = random();
                write_volatile_safe(first_ref, val);
                write_volatile_safe(second_ref, val);
            }
        }
    }

    macro_rules! two_region_write_fn_with_transform_fn {
        ($transform_fn:expr) => {
            |_state, first_ref, second_ref| {
                let mixing_val: usize = random();

                let val = read_volatile_safe(first_ref);
                let new_val = $transform_fn(val, mixing_val);
                write_volatile_safe(first_ref, new_val);

                let val = read_volatile_safe(second_ref);
                let new_val = $transform_fn(val, mixing_val);
                write_volatile_safe(second_ref, new_val);
            }
        };
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the XOR result of a random value and the value
    /// read from the location. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct Xor {}
    impl TwoRegionTestAlgorithm for Xor {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(std::ops::BitXor::bitxor)
        }
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the result of subtracting a random value from
    /// the value read from the location. After all locations are written, read and compare the two
    /// halves.
    #[derive(Debug, Default)]
    pub(super) struct Sub {}
    impl TwoRegionTestAlgorithm for Sub {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(usize::wrapping_sub)
        }
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the result of multiplying a random value with
    /// the value read from the location. After all locations are written, read and compare the two
    /// halves.
    #[derive(Debug, Default)]
    pub(super) struct Mul {}
    impl TwoRegionTestAlgorithm for Mul {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(usize::wrapping_mul)
        }
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the result of dividing the value read from the
    /// location with a random value. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct Div {}
    impl TwoRegionTestAlgorithm for Div {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(
                |n: usize, d: usize| n.wrapping_div(usize::max(d, 1))
            )
        }
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the OR result of a random value and the value
    /// read from the location. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct Or {}
    impl TwoRegionTestAlgorithm for Or {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(std::ops::BitOr::bitor)
        }
    }

    /// Reset all bits in given memory to 1s. Split given memory into two halves and iterate through
    /// memory locations in pairs. For each pair, write the AND result of a random value and the value
    /// read from the location. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct And {}
    impl TwoRegionTestAlgorithm for And {
        fn reset_before_run(&self) -> bool {
            true
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            two_region_write_fn_with_transform_fn!(std::ops::BitAnd::bitand)
        }
    }

    /// Split given memory into two halves and iterate through memory locations in pairs. Generate a
    /// random value at the start. For each pair, write the result of adding the random value and the
    /// index of iteration. After all locations are written, read and compare the two halves.
    #[derive(Debug, Default)]
    pub(super) struct SeqInc {
        val: usize,
    }
    impl TwoRegionTestAlgorithm for SeqInc {
        fn reset_before_run(&self) -> bool {
            false
        }
        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            |state, first_ref, second_ref| {
                state.val = state.val.wrapping_add(1);
                write_volatile_safe(first_ref, state.val);
                write_volatile_safe(second_ref, state.val);
            }
        }
    }

    /// Split given memory into two halves and iterate through memory locations in pairs. For each
    /// pair, write to all bits as either 1s or 0s, alternating after each memory location pair.
    /// After all locations are written, read and compare the two halves.
    /// This procedure is repeated 64 times.
    #[derive(Debug)]
    pub(super) struct SolidBits {
        solid_bits: usize,
        val: usize,
    }

    impl Default for SolidBits {
        fn default() -> Self {
            SolidBits {
                solid_bits: !0,
                val: usize::default(),
            }
        }
    }

    impl TwoRegionTestAlgorithm for SolidBits {
        fn num_runs(&self) -> u64 {
            64
        }

        fn start_run(&mut self, _run_idx: u64) {
            self.solid_bits = !self.solid_bits;
            self.val = self.solid_bits;
        }

        fn reset_before_run(&self) -> bool {
            false
        }

        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            |state, first_ref, second_ref| {
                state.val = !state.val;
                write_volatile_safe(first_ref, state.val);
                write_volatile_safe(second_ref, state.val);
            }
        }
    }

    /// Split given memory into two halves and iterate through memory locations in pairs. For each pair,
    /// write to a pattern of alternating 1s and 0s (in bytes it is either 0x55 or 0xaa, and alternating
    /// after each memory location pair). After all locations are written, read and compare the two
    /// halves.
    /// This procedure is repeated 64 times.
    #[derive(Debug)]
    pub(super) struct Checkerboard {
        checker_board: usize,
        val: usize,
    }

    impl Default for Checkerboard {
        fn default() -> Self {
            Checkerboard {
                checker_board: usize_filled_from_byte(0xaa),
                val: usize::default(),
            }
        }
    }

    impl TwoRegionTestAlgorithm for Checkerboard {
        fn num_runs(&self) -> u64 {
            64
        }

        fn start_run(&mut self, _run_idx: u64) {
            self.checker_board = !self.checker_board;
            self.val = self.checker_board;
        }

        fn reset_before_run(&self) -> bool {
            false
        }

        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            |state, first_ref, second_ref| {
                state.val = !state.val;
                write_volatile_safe(first_ref, state.val);
                write_volatile_safe(second_ref, state.val);
            }
        }
    }

    /// Split given memory into two halves and iterate through memory locations in pairs. For each pair,
    /// write to all bytes with the value i. After all locations are written, read and compare the two
    /// halves.
    /// This procedure is repeated 256 times, with i corresponding to the iteration number 0-255.
    #[derive(Debug, Default)]
    pub(super) struct BlockSeq {
        val: usize,
    }

    impl TwoRegionTestAlgorithm for BlockSeq {
        fn num_runs(&self) -> u64 {
            256
        }

        fn start_run(&mut self, run_idx: u64) {
            self.val = usize_filled_from_byte(run_idx.try_into().unwrap());
        }

        fn reset_before_run(&self) -> bool {
            false
        }

        fn write_fn(&self) -> TwoRegionWriteFn<Self> {
            |state, first_ref, second_ref| {
                write_volatile_safe(first_ref, state.val);
                write_volatile_safe(second_ref, state.val);
            }
        }
    }
}

mod mov_inv {
    use {
        super::{
            address_from_ref, check_expected_value, read_volatile_safe, usize_filled_from_byte,
            write_volatile_safe, IterFn, PassDirection, PassFn, TestAlgorithm,
        },
        rand::{random, rngs::SmallRng, Rng, SeedableRng},
        std::{
            fmt,
            time::{SystemTime, UNIX_EPOCH},
        },
    };

    /// These tests adapt the moving inversion algorithm implemented by [memtest86+](https://github.com/
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
    pub(super) trait MovInvAlgorithm: fmt::Debug + Default {
        fn num_runs(&self) -> u64;

        fn start_run(&mut self, _run_idx: u64);

        fn start_pass(&mut self, _pass_idx: u64) {}

        fn generate_pattern(&mut self) -> usize;

        fn backtrack_pattern(&mut self) -> usize;

        fn passes(&self) -> Vec<PassFn<Self>> {
            vec![Self::pass_0, Self::pass_1, Self::pass_2]
        }

        fn pass_0(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
            self.start_pass(0);
            |state, mem_ref| {
                let pattern = state.generate_pattern();
                write_volatile_safe(mem_ref, pattern);
                Ok(())
            }
        }

        fn pass_1(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
            self.start_pass(1);

            |state, mem_ref| {
                let pattern = state.generate_pattern();
                check_expected_value(
                    address_from_ref(mem_ref),
                    pattern,
                    read_volatile_safe(mem_ref),
                )?;

                write_volatile_safe(mem_ref, !pattern);
                Ok(())
            }
        }

        fn pass_2(&mut self, direction: &mut PassDirection) -> IterFn<Self> {
            *direction = PassDirection::Reverse;

            self.start_pass(2);

            |state, mem_ref| {
                let pattern = !state.backtrack_pattern();
                check_expected_value(
                    address_from_ref(mem_ref),
                    pattern,
                    read_volatile_safe(mem_ref),
                )?;

                write_volatile_safe(mem_ref, !pattern);
                Ok(())
            }
        }
    }

    impl<T: MovInvAlgorithm> TestAlgorithm for T {
        fn num_runs(&self) -> u64 {
            T::num_runs(self)
        }
        fn start_run(&mut self, run_idx: u64) {
            T::start_run(self, run_idx)
        }
        fn passes(&self) -> Vec<PassFn<Self>> {
            T::passes(self)
        }
    }

    #[derive(Debug, Default)]
    pub(super) struct FixedBlock {
        run_idx: u64,
    }

    /// This test runs the moving inversion algorithm with fixed patterns of all bits as 1s or 0s.
    impl MovInvAlgorithm for FixedBlock {
        fn num_runs(&self) -> u64 {
            2
        }

        fn start_run(&mut self, run_idx: u64) {
            self.run_idx = run_idx;
        }

        fn generate_pattern(&mut self) -> usize {
            self.pattern()
        }

        fn backtrack_pattern(&mut self) -> usize {
            self.pattern()
        }
    }

    impl FixedBlock {
        fn pattern(&mut self) -> usize {
            if self.run_idx == 0 {
                0
            } else {
                !0
            }
        }
    }

    /// This test runs the moving inversion algorithm with fixed 8-bit patterns where 1 bit is 1/0 and the
    /// other 7 bits are 0/1s.  The procedure is repeated 8 times with the pattern rotated by 1 bit each
    /// time to test all bits in a byte.
    #[derive(Debug)]
    pub(super) struct FixedBit {
        run_idx: u64,
        pattern: usize,
    }

    impl Default for FixedBit {
        fn default() -> Self {
            FixedBit {
                run_idx: u64::default(),
                pattern: usize_filled_from_byte(0x01),
            }
        }
    }

    impl MovInvAlgorithm for FixedBit {
        fn num_runs(&self) -> u64 {
            16
        }

        fn start_run(&mut self, run_idx: u64) {
            self.run_idx = run_idx;
            if run_idx % 2 == 0 {
                self.pattern = self.pattern.rotate_right(1);
            }
        }

        fn generate_pattern(&mut self) -> usize {
            self.pattern()
        }

        fn backtrack_pattern(&mut self) -> usize {
            self.pattern()
        }
    }

    impl FixedBit {
        fn pattern(&mut self) -> usize {
            if self.run_idx % 2 == 0 {
                self.pattern
            } else {
                !self.pattern
            }
        }
    }

    /// This test runs the moving inversion algorithm with a random fixed pattern.
    #[derive(Debug)]
    pub(super) struct FixedRandom {
        run_idx: u64,
        pattern: usize,
    }

    impl Default for FixedRandom {
        fn default() -> Self {
            FixedRandom {
                run_idx: u64::default(),
                pattern: random(),
            }
        }
    }

    impl MovInvAlgorithm for FixedRandom {
        fn num_runs(&self) -> u64 {
            2
        }

        fn start_run(&mut self, run_idx: u64) {
            self.run_idx = run_idx;
        }

        fn generate_pattern(&mut self) -> usize {
            self.pattern()
        }

        fn backtrack_pattern(&mut self) -> usize {
            self.pattern()
        }
    }

    impl FixedRandom {
        fn pattern(&mut self) -> usize {
            if self.run_idx % 2 == 0 {
                self.pattern
            } else {
                !self.pattern
            }
        }
    }

    #[derive(Debug, Default)]
    pub(super) struct Walk {
        run_idx: u64,
        starting_pattern: usize,
        pattern: usize,
    }

    /// This test runs the moving inversion algorithm with a "walking" bit pattern. The algorithm starts
    /// with 0x1 (or the compliment of 0x1) and "walks" the bit by shifting left for every new memory
    /// location.
    /// The procedure is repeated with offsets 0-31 or 0-63 depending on the size of `usize` to test all
    /// bits in a memory location.
    impl MovInvAlgorithm for Walk {
        fn num_runs(&self) -> u64 {
            u64::from(usize::BITS) * 2
        }

        fn start_run(&mut self, run_idx: u64) {
            self.run_idx = run_idx;
            self.starting_pattern = 1 << (run_idx / 2);
        }

        fn start_pass(&mut self, pass_idx: u64) {
            // For pass_idx == 2, pattern is not reset
            if pass_idx == 0 || pass_idx == 1 {
                self.pattern = self.starting_pattern;
            }
        }

        fn generate_pattern(&mut self) -> usize {
            let pattern = if self.run_idx % 2 == 0 {
                self.pattern
            } else {
                !self.pattern
            };
            self.pattern = self.pattern.rotate_left(1);
            pattern
        }

        fn backtrack_pattern(&mut self) -> usize {
            self.pattern = self.pattern.rotate_right(1);
            if self.run_idx % 2 == 0 {
                self.pattern
            } else {
                !self.pattern
            }
        }
    }

    // Despite its name MovInvRandom, this implementation (which follows the Memtest86+
    // implementation) is not similar to other moving inversion tests. Due to its nature of using
    // random numbers for each address, pass 2 does not traverse the memory in reverse.
    // TODO: Consider using a reversible RNG to match other moving inversion tests
    // See https://stackoverflow.com/questions/31513168/finding-inverse-operation-to-george-marsaglias-xorshift-rng
    // and https://stackoverflow.com/questions/31521910/simplify-the-inverse-of-z-x-x-y-function/31522122#31522122
    // for XORshift, which is the RNG used in Memtest86+
    #[derive(Debug)]
    pub(super) struct Random {
        seed: u64,
        rng: SmallRng,
    }

    impl Default for Random {
        fn default() -> Self {
            let seed = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_millis() as u64;
            Random {
                seed,
                rng: SmallRng::seed_from_u64(seed),
            }
        }
    }

    impl TestAlgorithm for Random {
        fn passes(&self) -> Vec<PassFn<Self>> {
            vec![Self::pass_0, Self::pass_1, Self::pass_2]
        }
    }

    impl Random {
        fn pass_0(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
            self.reset_rng();

            |state, mem_ref| {
                let pattern = state.rng.gen();
                write_volatile_safe(mem_ref, pattern);

                Ok(())
            }
        }

        fn pass_1(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
            self.reset_rng();

            |state, mem_ref| {
                let pattern = state.rng.gen();
                check_expected_value(
                    address_from_ref(mem_ref),
                    pattern,
                    read_volatile_safe(mem_ref),
                )?;

                write_volatile_safe(mem_ref, !pattern);
                Ok(())
            }
        }

        fn pass_2(&mut self, _direction: &mut PassDirection) -> IterFn<Self> {
            self.reset_rng();

            |state, mem_ref| {
                let pattern = !state.rng.gen::<usize>();
                check_expected_value(
                    address_from_ref(mem_ref),
                    pattern,
                    read_volatile_safe(mem_ref),
                )?;

                write_volatile_safe(mem_ref, !pattern);
                Ok(())
            }
        }

        fn reset_rng(&mut self) {
            self.rng = SmallRng::seed_from_u64(self.seed);
        }
    }
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
pub fn run_block_move<O: TestObserver>(memory: &mut [usize], mut observer: O) -> TestResult<O> {
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
            observer.check().map_err(Error::Observer)?;

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
                observer.check().map_err(Error::Observer)?;

                if let Err(f) = check_expected_value(
                    address_from_ref(mem_ref),
                    expected,
                    read_volatile_safe(mem_ref),
                ) {
                    return Ok(Outcome::Fail(f));
                }
            }
        }
        pattern = pattern.rotate_left(1);
    }

    Ok(Outcome::Pass)
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

/// This test uses the Modulo-20 algorithm implemented by [memtest86+](https://github.com/
/// memtest86plus/memtest86plus),  which is designed to avoid effects of caching and buffering.
///
/// The test generates a random value, then write the value to every 20th memory location.
/// Afterwards write the complement of the value to all other locations one or more times (twice in
/// this case). Then verify that the values stored in every 20th location is unchanged.
///
/// The procedure is repeated with offsets 0-19 to test all memory locations.
#[tracing::instrument(skip_all)]
pub fn run_modulo_20<O: TestObserver>(memory: &mut [usize], mut observer: O) -> TestResult<O> {
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
            observer.check().map_err(Error::Observer)?;
            write_volatile_safe(mem_ref, pattern);
        }

        for _ in 0..2 {
            for (i, mem_ref) in memory.iter_mut().enumerate() {
                if i % STEP == offset {
                    continue;
                }
                observer.check().map_err(Error::Observer)?;
                write_volatile_safe(mem_ref, !pattern);
            }
        }

        for mem_ref in memory.iter().skip(offset).step_by(STEP) {
            observer.check().map_err(Error::Observer)?;

            if let Err(f) = check_expected_value(
                address_from_ref(mem_ref),
                pattern,
                read_volatile_safe(mem_ref),
            ) {
                return Ok(Outcome::Fail(f));
            }
        }
    }

    Ok(Outcome::Pass)
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

impl fmt::Debug for Failure {
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

impl fmt::Display for ParseTestKindError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl error::Error for ParseTestKindError {}

impl fmt::Display for Outcome {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Outcome: {:?}", self)
    }
}

impl<E: fmt::Debug> fmt::Display for Error<E> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Error: {:?}", self)
    }
}

impl<E: error::Error + 'static> error::Error for Error<E> {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::Observer(err) => Some(err),
            Error::Other(err) => Some(err.as_ref()),
        }
    }
}

impl<E> From<anyhow::Error> for Error<E> {
    fn from(err: anyhow::Error) -> Error<E> {
        Error::Other(err)
    }
}

fn serialize_error_other<S>(error: &anyhow::Error, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    serializer.serialize_str(&format!("{:?}", error))
}

fn deserialize_error_other<'de, D>(deserializer: D) -> Result<anyhow::Error, D::Error>
where
    D: Deserializer<'de>,
{
    let str = String::deserialize(deserializer)?;
    Ok(anyhow!(str))
}

#[cfg(test)]
mod test {
    use {
        super::{Outcome, TestKind, TestObserver},
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
        for test_kind in TestKind::ALL {
            let mut counter = IterationCounter::new();
            assert!(
                matches!(test_kind.run(&mut memory, &mut counter), Ok(Outcome::Pass),),
                "{:?} should pass",
                test_kind
            );
            counter.assert_count();
        }
    }
}
