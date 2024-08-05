use std::borrow::Cow;

use minidump::system_info::{Cpu, Os};

/// Information about the system that produced a `Minidump`.
#[derive(Debug, Clone)]
pub struct SystemInfo {
    /// The operating system that produced the minidump
    pub os: Os,
    /// A string identifying the version of the operating system.
    ///
    /// This may look like "5.1.2600" or "10.4.8", if present
    pub os_version: Option<String>,
    /// A string identifying the exact build of the operating system.
    ///
    /// This may look like "Service Pack 2" or "8L2127", if present. On Windows,
    /// this is the CSD version, on Linux, extended build information and macOS,
    /// the product build version.
    pub os_build: Option<String>,
    /// The CPU on which the dump was produced
    pub cpu: Cpu,
    /// A string further identifying the specific CPU
    ///
    /// For example,  "GenuineIntel level 6 model 13 stepping 8", if present.
    pub cpu_info: Option<String>,
    /// The microcode version of the cpu
    pub cpu_microcode_version: Option<u64>,
    /// The number of processors in the system
    ///
    /// Will be greater than one for multi-core systems.
    pub cpu_count: usize,
}

impl SystemInfo {
    /// Returns the full available operating system version.
    ///
    /// Returns the version and the build, if available, otherwise just the version.
    pub fn format_os_version(&self) -> Option<Cow<'_, str>> {
        match (&self.os_version, &self.os_build) {
            (Some(v), Some(b)) => Some(format!("{v} {b}").into()),
            (Some(v), None) => Some(Cow::Borrowed(v)),
            (None, Some(b)) => Some(Cow::Borrowed(b)),
            (None, None) => None,
        }
    }
}
