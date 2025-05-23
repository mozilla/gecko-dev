#[cfg(unix)]
use unix::{memory_lock, memory_resize_and_lock, PageFaultChecker};
#[cfg(windows)]
use windows::{memory_lock, memory_resize_and_lock, replace_set_size};
use {
    prelude::*,
    rand::{seq::SliceRandom, thread_rng},
    serde::{Deserialize, Serialize},
    std::{
        error, fmt,
        time::{Duration, Instant},
    },
};

mod memtest;
mod prelude;

pub use memtest::{Error, Failure, Outcome, ParseTestKindError, TestKind, TestResult};

#[derive(Debug)]
pub struct Runner {
    test_kinds: Vec<TestKind>,
    timeout: Duration,
    mem_lock_mode: MemLockMode,
    #[allow(dead_code)]
    allow_working_set_resize: bool,
    allow_multithread: bool,
    allow_early_termination: bool,
}

// TODO: Replace RunnerArgs with a Builder struct implementing fluent interface
/// A set of arguments that define the behavior of Runner
#[derive(Serialize, Deserialize, Debug)]
pub struct RunnerArgs {
    /// How long should Runner run the test suite before timing out
    pub timeout: Duration,
    /// Whether memory will be locked before testing and whether the requested memory size of
    /// testing can be reduced to accomodate memory locking
    /// If memory locking failed but is required, Runner returns with error
    pub mem_lock_mode: MemLockMode,
    /// Whether the process working set can be resized to accomodate memory locking
    /// This argument is only meaningful for Windows
    pub allow_working_set_resize: bool,
    /// Whether mulithreading is enabled
    pub allow_multithread: bool,
    /// Whether Runner returns immediately if a test fails or continues until all tests are run
    pub allow_early_termination: bool,
}

#[derive(Debug)]
pub enum RunnerError {
    MemLockFailed(anyhow::Error),
    Other(anyhow::Error),
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TestReportList {
    pub tested_mem_length: usize,
    pub mlocked: bool,
    pub reports: Vec<TestReport>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TestReport {
    pub test_kind: TestKind,
    pub outcome: Result<Outcome, memtest::Error<RuntimeError>>,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum MemLockMode {
    Resizable,
    FixedSize,
    PageFaultChecking,
    Disabled,
}

#[derive(Debug, PartialEq, Eq)]
pub struct ParseMemLockModeError;

/// The minimum memory length (in usize) for Runner to run tests on
/// On a 64-bit machine, this is the size of a page
pub const MIN_MEMORY_LENGTH: usize = 512;

#[derive(Debug)]
struct MemLockGuard {
    base_ptr: *mut usize,
    mem_size: usize,
}

#[derive(Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum RuntimeError {
    Timeout,
    PageFault,
}

/// A struct to ensure the test timeouts in a given duration
#[derive(Debug)]
struct TimeoutChecker {
    deadline: Instant,
    state: Option<TimeoutCheckerState>,
}

#[derive(Debug)]
struct TimeoutCheckerState {
    test_start_time: Instant,
    expected_iter: u64,
    completed_iter: u64,
    checkpoint: u64,
}

impl Runner {
    /// Create a Runner containing all test kinds in random order
    pub fn all_tests_random_order(args: &RunnerArgs) -> Runner {
        let mut test_kinds = TestKind::ALL.to_vec();
        test_kinds.shuffle(&mut thread_rng());

        Self::from_test_kinds(args, test_kinds)
    }

    /// Create a Runner with specified test kinds
    pub fn from_test_kinds(args: &RunnerArgs, test_kinds: Vec<TestKind>) -> Runner {
        Runner {
            test_kinds,
            timeout: args.timeout,
            mem_lock_mode: args.mem_lock_mode,
            allow_working_set_resize: args.allow_working_set_resize,
            allow_multithread: args.allow_multithread,
            allow_early_termination: args.allow_early_termination,
        }
    }

    /// Run the tests, possibly after locking the memory
    pub fn run(&self, memory: &mut [usize]) -> Result<TestReportList, RunnerError> {
        if memory.len() < MIN_MEMORY_LENGTH {
            return Err(anyhow!("Insufficient memory length").into());
        }

        let deadline = Instant::now() + self.timeout;

        if matches!(
            self.mem_lock_mode,
            MemLockMode::Disabled | MemLockMode::PageFaultChecking
        ) {
            return Ok(TestReportList {
                tested_mem_length: memory.len(),
                mlocked: false,
                reports: self.run_tests(memory, deadline),
            });
        }

        #[cfg(windows)]
        // TODO: When it is MemLockMode::Resizable and working set resize failed, consider shrinking
        // the memory region and try again
        let _working_set_resize_guard = if self.allow_working_set_resize {
            Some(
                replace_set_size(std::mem::size_of_val(memory))
                    .context("Failed to replace process working set size")?,
            )
        } else {
            None
        };

        let (memory, _mem_lock_guard) = match self.mem_lock_mode {
            MemLockMode::FixedSize => memory_lock(memory),
            MemLockMode::Resizable => memory_resize_and_lock(memory),
            _ => unreachable!(),
        }
        .map_err(RunnerError::MemLockFailed)?;

        Ok(TestReportList {
            tested_mem_length: memory.len(),
            mlocked: true,
            reports: self.run_tests(memory, deadline),
        })
    }

    /// Run tests
    fn run_tests(&self, memory: &mut [usize], deadline: Instant) -> Vec<TestReport> {
        let mut reports = Vec::new();
        let mut timed_out = false;

        for test_kind in &self.test_kinds {
            let test_result = if timed_out {
                Err(memtest::Error::Observer(RuntimeError::Timeout))
            } else if self.allow_multithread {
                std::thread::scope(|scope| {
                    let num_threads = num_cpus::get();
                    let chunk_size = memory.len() / num_threads;

                    let mut handles = vec![];
                    for chunk in memory.chunks_exact_mut(chunk_size) {
                        handles.push(scope.spawn(|| self.run_test(*test_kind, chunk, deadline)));
                    }

                    #[allow(clippy::manual_try_fold)]
                    handles
                        .into_iter()
                        .map(|handle| {
                            handle
                                .join()
                                .unwrap_or(Err(memtest::Error::Other(anyhow!("Thread panicked"))))
                        })
                        .fold(Ok(Outcome::Pass), |acc, result| {
                            use {memtest::Error::*, Outcome::*};
                            match (acc, result) {
                                (Err(Other(e)), _) | (_, Err(Other(e))) => Err(Other(e)),
                                (Ok(Fail(f)), _) | (_, Ok(Fail(f))) => Ok(Fail(f)),
                                (Err(Observer(e)), _) | (_, Err(Observer(e))) => Err(Observer(e)),
                                _ => Ok(Pass),
                            }
                        })
                })
            } else {
                self.run_test(*test_kind, memory, deadline)
            };
            timed_out = matches!(
                test_result,
                Err(memtest::Error::Observer(RuntimeError::Timeout))
            );

            if matches!(test_result, Ok(Outcome::Fail(_))) && self.allow_early_termination {
                reports.push(TestReport::new(*test_kind, test_result));
                warn!("Test failed, terminating early");
                break;
            }
            reports.push(TestReport::new(*test_kind, test_result));
        }

        reports
    }

    fn run_test(
        &self,
        test_kind: TestKind,
        memory: &mut [usize],
        deadline: Instant,
    ) -> Result<Outcome, memtest::Error<RuntimeError>> {
        let timeout_checker = TimeoutChecker::new(deadline);

        #[cfg(unix)]
        if matches!(self.mem_lock_mode, MemLockMode::PageFaultChecking) {
            let runtime_checker = unix::RuntimeChecker::new(
                timeout_checker,
                PageFaultChecker::new(memory.as_mut_ptr() as usize, memory.len())
                    .map_err(memtest::Error::Other)?,
            );
            test_kind.run(memory, runtime_checker)
        } else {
            test_kind.run(memory, timeout_checker)
        }

        // PageFaultChecker is not implemented for Windows
        #[cfg(windows)]
        test_kind.run(memory, timeout_checker)
    }
}

impl fmt::Display for RunnerError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl error::Error for RunnerError {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            RunnerError::MemLockFailed(err) | RunnerError::Other(err) => Some(err.as_ref()),
        }
    }
}

impl From<anyhow::Error> for RunnerError {
    fn from(err: anyhow::Error) -> RunnerError {
        RunnerError::Other(err)
    }
}

impl std::str::FromStr for MemLockMode {
    type Err = ParseMemLockModeError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "resizable" => Ok(Self::Resizable),
            "fixed_size" => Ok(Self::FixedSize),
            "page_fault_checking" => Ok(Self::PageFaultChecking),
            "disabled" => Ok(Self::Disabled),
            _ => Err(ParseMemLockModeError),
        }
    }
}

impl fmt::Display for ParseMemLockModeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl error::Error for ParseMemLockModeError {}

impl fmt::Display for TestReportList {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "tested_mem_len = {}", self.tested_mem_length)?;
        writeln!(f, "mlocked = {}", self.mlocked)?;
        for report in &self.reports {
            let outcome = match &report.outcome {
                Ok(outcome) => format!("{}", outcome),
                Err(e) => format!("{}", e),
            };
            writeln!(
                f,
                "{:<30} {}",
                format!("Ran Test: {:?}", report.test_kind),
                outcome
            )?;
        }
        Ok(())
    }
}

impl TestReportList {
    pub fn iter(&self) -> impl Iterator<Item = &TestReport> {
        self.reports.iter()
    }

    /// Returns true if all tests were run successfully and all tests passed
    pub fn all_pass(&self) -> bool {
        self.reports
            .iter()
            .all(|report| matches!(report.outcome, Ok(Outcome::Pass)))
    }
}

impl TestReport {
    fn new(
        test_kind: TestKind,
        outcome: Result<Outcome, memtest::Error<RuntimeError>>,
    ) -> TestReport {
        TestReport { test_kind, outcome }
    }
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl error::Error for RuntimeError {}

impl TimeoutChecker {
    fn new(deadline: Instant) -> TimeoutChecker {
        TimeoutChecker {
            deadline,
            state: None,
        }
    }
}

impl memtest::TestObserver for TimeoutChecker {
    type Error = RuntimeError;

    /// Initialize TimeoutCheckerState
    /// This function should be called in the beginning of a memtest.
    fn init(&mut self, expected_iter: u64) {
        const FIRST_CHECKPOINT: u64 = 8;

        assert!(
            self.state.is_none(),
            "init() should only be called once per test"
        );

        // The first checkpoint is set to 8 to have a more accurate sample of duration per
        // iteration for determining new checkpoint
        self.state = Some(TimeoutCheckerState {
            test_start_time: Instant::now(),
            expected_iter,
            completed_iter: 0,
            checkpoint: FIRST_CHECKPOINT,
        });
    }

    /// Check if the current iteration is a checkpoint. If so, check if timeout occurred
    ///
    /// This function should be called in every iteration of a memtest.
    ///
    /// To reduce overhead, the function only checks for timeout at specific checkpoints, and
    /// early returns otherwise.
    ///
    // It is important to ensure that the "early return" hot path is inlined. This results in a
    // 100% improvement in performance.
    #[inline(always)]
    fn check(&mut self) -> Result<(), Self::Error> {
        let state = self
            .state
            .as_mut()
            .expect("init() should be called before check()");

        if state.completed_iter < state.checkpoint {
            state.completed_iter += 1;
            return Ok(());
        }

        state.on_checkpoint(self.deadline)
    }
}

impl TimeoutCheckerState {
    fn on_checkpoint(&mut self, deadline: Instant) -> Result<(), RuntimeError> {
        let current_time = Instant::now();
        if current_time >= deadline {
            return Err(RuntimeError::Timeout);
        }

        self.trace_progress();
        self.set_next_checkpoint(deadline, current_time);

        self.completed_iter += 1;
        Ok(())
    }

    // TODO: Consider separating this functionality to a `ProgressTracer`
    // Note: Because `trace_progress()` is only called in `on_checkpoints()`, not every percent
    // of the test progress is traced. If memtests are running way ahead of the given deadline, the
    // progress may only be traced once or twice. Although this makes the logs less comprehensive,
    // it avoids signficiant perforamnce overhead.
    fn trace_progress(&mut self) {
        if tracing::enabled!(tracing::Level::TRACE) {
            trace!(
                "Progress on checkpoint: {:.2}%",
                self.completed_iter as f64 / self.expected_iter as f64 * 100.0
            );
        }
    }

    /// Calculate the remaining time before the deadline and schedule the next check at 75% of that
    /// interval, then estimate the number of iterations to get there and set as next checkpoint
    fn set_next_checkpoint(&mut self, deadline: Instant, current_time: Instant) {
        const DEADLINE_CHECK_RATIO: f64 = 0.75;

        let duration_until_next_checkpoint = {
            let duration_until_deadline = deadline - current_time;
            duration_until_deadline.mul_f64(DEADLINE_CHECK_RATIO)
        };

        let avg_iter_duration = {
            let test_elapsed = current_time - self.test_start_time;
            test_elapsed.div_f64(self.completed_iter as f64)
        };

        let iter_until_next_checkpoint = {
            let x =
                Self::div_duration_f64(duration_until_next_checkpoint, avg_iter_duration) as u64;
            u64::max(x, 1)
        };

        self.checkpoint += iter_until_next_checkpoint;
    }

    // This is equivalent to `Duration::div_duration_f64`, but that is not stable on Rust 1.76
    fn div_duration_f64(lhs: Duration, rhs: Duration) -> f64 {
        const NANOS_PER_SEC: u32 = 1_000_000_000;
        let lhs_nanos =
            (lhs.as_secs() as f64) * (NANOS_PER_SEC as f64) + (lhs.subsec_nanos() as f64);
        let rhs_nanos =
            (rhs.as_secs() as f64) * (NANOS_PER_SEC as f64) + (rhs.subsec_nanos() as f64);
        lhs_nanos / rhs_nanos
    }
}

#[cfg(windows)]
mod windows {
    use {
        crate::{prelude::*, MemLockGuard},
        std::mem::{size_of, size_of_val},
        windows::Win32::{
            Foundation::ERROR_WORKING_SET_QUOTA,
            System::{
                Memory::{VirtualLock, VirtualUnlock},
                SystemInformation::{
                    GetNativeSystemInfo, GlobalMemoryStatusEx, MEMORYSTATUSEX, SYSTEM_INFO,
                },
                Threading::{
                    GetCurrentProcess, GetProcessWorkingSetSize, SetProcessWorkingSetSize,
                },
            },
        },
    };

    #[derive(Debug)]
    pub struct WorkingSetResizeGuard {
        min_set_size: usize,
        max_set_size: usize,
    }

    // TODO: Consider verifying that the process memory is properly sized by using
    // `GetProcessMemoryInfo` during memtests to retrieve the number of page faults this process is
    // causing. If it's suddenly a very high number, it indicates the set size might be too small
    pub(super) fn replace_set_size(memsize: usize) -> anyhow::Result<WorkingSetResizeGuard> {
        const ESTIMATED_TEST_MEM_USAGE: usize = 1024 * 1024; // 1MiB
        let (min_set_size, max_set_size) = get_set_size()?;
        let new_min_set_size = memsize + ESTIMATED_TEST_MEM_USAGE;
        let new_max_set_size: usize = get_physical_memory_size()
            .context("Failed to get physical memory size")?
            .try_into()
            .unwrap_or(usize::MAX);
        unsafe {
            SetProcessWorkingSetSize(GetCurrentProcess(), new_min_set_size, new_max_set_size)
                .context("Failed to set process working set size")?;
        }
        Ok(WorkingSetResizeGuard {
            min_set_size,
            max_set_size,
        })
    }

    impl Drop for WorkingSetResizeGuard {
        fn drop(&mut self) {
            unsafe {
                if let Err(e) = SetProcessWorkingSetSize(
                    GetCurrentProcess(),
                    self.min_set_size,
                    self.max_set_size,
                ) {
                    warn!("Failed to restore process working set: {e}");
                }
            }
        }
    }

    pub(super) fn memory_lock(
        memory: &mut [usize],
    ) -> anyhow::Result<(&mut [usize], MemLockGuard)> {
        let base_ptr = memory.as_mut_ptr();
        let mem_size = size_of_val(memory);

        unsafe {
            VirtualLock(base_ptr.cast(), mem_size).context("VirtualLock failed")?;
        }
        info!("Successfully locked {}MB", mem_size);
        Ok((memory, MemLockGuard { base_ptr, mem_size }))
    }

    pub(super) fn memory_resize_and_lock(
        mut memory: &mut [usize],
    ) -> anyhow::Result<(&mut [usize], MemLockGuard)> {
        // Resizing to system limit first is more efficient than only decrementing by page
        // size and retry locking.
        let min_set_size_usize = get_set_size()?.0 / size_of::<usize>();
        if memory.len() > min_set_size_usize {
            memory = &mut memory[0..min_set_size_usize];
            warn!(
                "Resized memory to system limit ({} bytes)",
                size_of_val(memory)
            );
        }

        let usize_per_page = get_page_size()? / std::mem::size_of::<usize>();
        loop {
            let base_ptr = memory.as_mut_ptr();
            let mem_size = size_of_val(memory);

            let res = unsafe { VirtualLock(base_ptr.cast(), mem_size) };
            let Err(e) = res else {
                info!("Successfully locked {} bytes", mem_size);
                return Ok((memory, MemLockGuard { base_ptr, mem_size }));
            };

            ensure!(
                e == ERROR_WORKING_SET_QUOTA.into(),
                anyhow!(e).context("VirtualLock failed")
            );

            // Locking with the system limit can still fail as the memory to be locked may not be
            // page aligned. In that case retry locking after decrement memory size by a page.
            let new_len = memory
                .len()
                .checked_sub(usize_per_page)
                .context("Failed to lock any memory, memory size has been decremented to 0")?;

            memory = &mut memory[0..new_len];
            warn!(
                "Decremented memory size to {} bytes, retry memory locking",
                new_len * usize_per_page
            );
        }
    }

    impl Drop for MemLockGuard {
        fn drop(&mut self) {
            unsafe {
                if let Err(e) = VirtualUnlock(self.base_ptr.cast(), self.mem_size) {
                    warn!("Failed to unlock memory: {e}")
                }
            }
        }
    }

    fn get_set_size() -> anyhow::Result<(usize, usize)> {
        let (mut min_set_size, mut max_set_size) = (0, 0);
        unsafe {
            GetProcessWorkingSetSize(GetCurrentProcess(), &mut min_set_size, &mut max_set_size)
                .context("Failed to get process working set")?;
        }
        Ok((min_set_size, max_set_size))
    }

    fn get_page_size() -> anyhow::Result<usize> {
        Ok((unsafe {
            let mut sysinfo: SYSTEM_INFO = std::mem::zeroed();
            GetNativeSystemInfo(&mut sysinfo);
            sysinfo.dwPageSize
        })
        .try_into()
        .unwrap())
    }

    fn get_physical_memory_size() -> anyhow::Result<u64> {
        let mut memory_status = MEMORYSTATUSEX::default();
        memory_status.dwLength = std::mem::size_of_val(&memory_status).try_into().unwrap();
        unsafe {
            GlobalMemoryStatusEx(&mut memory_status)
                .context("Failed to get global memory status")?
        };
        Ok(memory_status.ullTotalPhys)
    }
}

#[cfg(unix)]
mod unix {
    use {
        crate::{memtest::TestObserver, prelude::*, MemLockGuard, RuntimeError, TimeoutChecker},
        libc::{getrlimit, mlock, munlock, rlimit, sysconf, RLIMIT_MEMLOCK, _SC_PAGESIZE},
        std::{
            borrow::BorrowMut,
            io::{self, ErrorKind},
            mem::{size_of, size_of_val},
        },
    };

    #[derive(Debug)]
    pub(super) struct RuntimeChecker {
        timeout_checker: TimeoutChecker,
        page_fault_checker: PageFaultChecker,
    }

    #[derive(Debug)]
    pub(super) struct PageFaultChecker {
        address: usize,
        len: usize,
        page_size: usize,
        state: Option<PageFaultCheckerState>,
    }

    #[derive(Debug)]
    struct PageFaultCheckerState {
        completed_iter: u64,
        checkpoint: u64,
        checkpoint_step: u64,
    }

    pub(super) fn memory_lock(
        memory: &mut [usize],
    ) -> anyhow::Result<(&mut [usize], MemLockGuard)> {
        let base_ptr = memory.as_mut_ptr();
        let mem_size = size_of_val(memory);
        if unsafe { mlock(base_ptr.cast(), mem_size) } == 0 {
            info!("Successfully locked {} bytes", mem_size);
            Ok((
                memory,
                MemLockGuard {
                    base_ptr: base_ptr.cast(),
                    mem_size,
                },
            ))
        } else {
            Err(anyhow!(io::Error::last_os_error()).context("mlock failed"))
        }
    }

    pub(super) fn memory_resize_and_lock(
        mut memory: &mut [usize],
    ) -> anyhow::Result<(&mut [usize], MemLockGuard)> {
        // Note: Resizing to system limit first is more efficient than only decrementing by page
        // size and retry locking, but this may not work as intended when running as a priviledged
        // process, since priviledged processes do not need to respect the limit.
        let max_mem_lock_usize = get_max_mem_lock()? / size_of::<usize>();
        if memory.len() > max_mem_lock_usize {
            memory = &mut memory[0..max_mem_lock_usize];
            warn!(
                "Resized memory to system limit ({} bytes)",
                size_of_val(memory)
            );
        }

        let usize_per_page = get_page_size()? / std::mem::size_of::<usize>();
        loop {
            let base_ptr = memory.as_mut_ptr();
            let mem_size = size_of_val(memory);
            if unsafe { mlock(base_ptr.cast(), mem_size) } == 0 {
                info!("Successfully locked {} bytes", mem_size);
                return Ok((memory, MemLockGuard { base_ptr, mem_size }));
            }

            let e = io::Error::last_os_error();
            ensure!(
                e.kind() == ErrorKind::OutOfMemory,
                anyhow!(e).context("mlock failed")
            );

            // Locking with the system limit can still fail as the memory to be locked may not be
            // page aligned. In that case retry locking after decrement memory size by a page.
            let new_len = memory
                .len()
                .checked_sub(usize_per_page)
                .context("Failed to lock any memory, memory size has been decremented to 0")?;
            memory = &mut memory[0..new_len];
            warn!(
                "Decremented memory size to {} bytes, retry memory locking",
                size_of_val(memory)
            );
        }
    }

    impl Drop for MemLockGuard {
        fn drop(&mut self) {
            unsafe {
                if munlock(self.base_ptr.cast(), self.mem_size) != 0 {
                    warn!("Failed to unlock memory: {}", io::Error::last_os_error())
                }
            }
        }
    }

    fn get_max_mem_lock() -> anyhow::Result<usize> {
        unsafe {
            let mut rlim: rlimit = std::mem::zeroed();
            ensure!(
                getrlimit(RLIMIT_MEMLOCK, rlim.borrow_mut()) == 0,
                anyhow!(io::Error::last_os_error()).context("Failed to get RLIMIT_MEMLOCK")
            );
            Ok(rlim.rlim_cur.try_into().unwrap())
        }
    }

    fn get_page_size() -> anyhow::Result<usize> {
        (unsafe { sysconf(_SC_PAGESIZE) })
            .try_into()
            .context("Failed to get page size")
    }

    impl RuntimeChecker {
        pub(super) fn new(
            timeout_checker: TimeoutChecker,
            page_fault_checker: PageFaultChecker,
        ) -> RuntimeChecker {
            RuntimeChecker {
                timeout_checker,
                page_fault_checker,
            }
        }
    }

    impl TestObserver for RuntimeChecker {
        type Error = RuntimeError;

        /// This function should be called in the beginning of a memtest.
        fn init(&mut self, expected_iter: u64) {
            self.timeout_checker.init(expected_iter);
            self.page_fault_checker.init(expected_iter);
        }

        #[inline(always)]
        fn check(&mut self) -> Result<(), Self::Error> {
            self.timeout_checker
                .check()
                .map_err(|_| RuntimeError::Timeout)?;
            self.page_fault_checker
                .check()
                .map_err(|_| RuntimeError::PageFault)?;
            Ok(())
        }
    }

    impl PageFaultChecker {
        pub(super) fn new(address: usize, len: usize) -> anyhow::Result<PageFaultChecker> {
            Ok(PageFaultChecker {
                address,
                len,
                page_size: get_page_size()?,
                state: None,
            })
        }
    }

    impl TestObserver for PageFaultChecker {
        type Error = RuntimeError;

        // TODO: Currently NUM_CHECKS is an arbitrary number
        fn init(&mut self, expected_iter: u64) {
            const FIRST_CHECKPOINT: u64 = 0;
            const NUM_CHECKS: u64 = 100;

            assert!(
                self.state.is_none(),
                "init() should only be called once per test"
            );

            self.state = Some(PageFaultCheckerState {
                completed_iter: 0,
                checkpoint: FIRST_CHECKPOINT,
                checkpoint_step: expected_iter / NUM_CHECKS,
            });
        }

        #[inline(always)]
        fn check(&mut self) -> Result<(), Self::Error> {
            let state = self
                .state
                .as_mut()
                .expect("init() should be called before check()");

            if state.completed_iter < state.checkpoint {
                state.completed_iter += 1;
                return Ok(());
            }

            state.on_checkpoint(self.address, self.len, self.page_size)
        }
    }

    impl PageFaultCheckerState {
        fn on_checkpoint(
            &mut self,
            address: usize,
            len: usize,
            page_size: usize,
        ) -> Result<(), RuntimeError> {
            use libc::mincore;

            let mem_size = len * size_of::<usize>();

            let aligned_address = address / page_size * page_size;
            let aligned_mem_size = mem_size + (address - aligned_address);

            // buf needs to be at least (aligned_mem_size + page_size - 1) / page_size
            let mut buf: Vec<u8> = vec![0; aligned_mem_size.div_ceil(page_size)];
            assert_eq!(
                unsafe {
                    mincore(
                        aligned_address as *mut std::ffi::c_void,
                        aligned_mem_size,
                        buf.as_mut_ptr().cast(),
                    )
                },
                0,
                "mincore did not return 0"
            );
            if buf.iter().any(|n| *n & 1 == 0) {
                trace!("Detected page fault");
                return Err(RuntimeError::PageFault);
            }

            self.set_next_checkpoint();
            self.completed_iter += 1;

            Ok(())
        }

        /// Calculate the remaining time before the deadline and schedule the next check at 75% of that
        /// interval, then estimate the number of iterations to get there and set as next checkpoint
        fn set_next_checkpoint(&mut self) {
            self.checkpoint += self.checkpoint_step;
        }
    }
}
