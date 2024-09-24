// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! Information about the system that produced a `Minidump`.

use num_traits::FromPrimitive;
use std::borrow::Cow;
use std::fmt;

use minidump_common::format as md;
use minidump_common::format::PlatformId;
use minidump_common::format::ProcessorArchitecture::*;

/// Known operating systems
///
/// This is a slightly nicer layer over the `PlatformId` enum defined in the minidump-common crate.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Os {
    Windows,
    MacOs,
    Ios,
    Linux,
    Solaris,
    Android,
    Ps3,
    NaCl,
    Unknown(u32),
}

impl Os {
    /// Get an `Os` value matching the `platform_id` value from `MINIDUMP_SYSTEM_INFO`
    pub fn from_platform_id(id: u32) -> Os {
        match PlatformId::from_u32(id) {
            Some(PlatformId::VER_PLATFORM_WIN32_WINDOWS)
            | Some(PlatformId::VER_PLATFORM_WIN32_NT) => Os::Windows,
            Some(PlatformId::MacOs) => Os::MacOs,
            Some(PlatformId::Ios) => Os::Ios,
            Some(PlatformId::Linux) => Os::Linux,
            Some(PlatformId::Solaris) => Os::Solaris,
            Some(PlatformId::Android) => Os::Android,
            Some(PlatformId::Ps3) => Os::Ps3,
            Some(PlatformId::NaCl) => Os::NaCl,
            _ => Os::Unknown(id),
        }
    }

    /// Get a human-readable friendly name for an `Os`
    pub fn long_name(&self) -> Cow<'_, str> {
        match *self {
            Os::Windows => Cow::Borrowed("Windows NT"),
            Os::MacOs => Cow::Borrowed("Mac OS X"),
            Os::Ios => Cow::Borrowed("iOS"),
            Os::Linux => Cow::Borrowed("Linux"),
            Os::Solaris => Cow::Borrowed("Solaris"),
            Os::Android => Cow::Borrowed("Android"),
            Os::Ps3 => Cow::Borrowed("PS3"),
            Os::NaCl => Cow::Borrowed("NaCl"),
            Os::Unknown(val) => Cow::Owned(format!("0x{val:#08x}")),
        }
    }
}

impl fmt::Display for Os {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match *self {
                Os::Windows => "windows",
                Os::MacOs => "mac",
                Os::Ios => "ios",
                Os::Linux => "linux",
                Os::Solaris => "solaris",
                Os::Android => "android",
                Os::Ps3 => "ps3",
                Os::NaCl => "nacl",
                Os::Unknown(_) => "unknown",
            }
        )
    }
}

/// Known CPU types
///
/// This is a slightly nicer layer over the `ProcessorArchitecture` enum defined in
/// the minidump-common crate.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Cpu {
    X86,
    X86_64,
    Ppc,
    Ppc64,
    Sparc,
    Arm,
    Arm64,
    Mips,
    Mips64,
    Unknown(u16),
}

/// Supported CPU pointer widths
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum PointerWidth {
    Bits32,
    Bits64,
    Unknown,
}

impl Cpu {
    /// Get a `Cpu` value matching the `processor_architecture` value from `MINIDUMP_SYSTEM_INFO`
    pub fn from_processor_architecture(arch: u16) -> Cpu {
        match md::ProcessorArchitecture::from_u16(arch) {
            Some(PROCESSOR_ARCHITECTURE_INTEL) | Some(PROCESSOR_ARCHITECTURE_IA32_ON_WIN64) => {
                Cpu::X86
            }
            Some(PROCESSOR_ARCHITECTURE_AMD64) => Cpu::X86_64,
            Some(PROCESSOR_ARCHITECTURE_PPC) => Cpu::Ppc,
            Some(PROCESSOR_ARCHITECTURE_PPC64) => Cpu::Ppc64,
            Some(PROCESSOR_ARCHITECTURE_SPARC) => Cpu::Sparc,
            Some(PROCESSOR_ARCHITECTURE_ARM) => Cpu::Arm,
            Some(PROCESSOR_ARCHITECTURE_ARM64) | Some(PROCESSOR_ARCHITECTURE_ARM64_OLD) => {
                Cpu::Arm64
            }
            Some(PROCESSOR_ARCHITECTURE_MIPS) => Cpu::Mips,
            Some(PROCESSOR_ARCHITECTURE_MIPS64) => Cpu::Mips64,
            _ => Cpu::Unknown(arch),
        }
    }

    /// The native pointer width of this platform
    pub fn pointer_width(&self) -> PointerWidth {
        match self {
            Cpu::X86 | Cpu::Ppc | Cpu::Sparc | Cpu::Arm | Cpu::Mips => PointerWidth::Bits32,
            Cpu::X86_64 | Cpu::Ppc64 | Cpu::Arm64 | Cpu::Mips64 => PointerWidth::Bits64,
            Cpu::Unknown(_) => PointerWidth::Unknown,
        }
    }
}

impl fmt::Display for Cpu {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match *self {
                Cpu::X86 => "x86",
                Cpu::X86_64 => "amd64",
                Cpu::Ppc => "ppc",
                Cpu::Ppc64 => "ppc64",
                Cpu::Sparc => "sparc",
                Cpu::Arm => "arm",
                Cpu::Arm64 => "arm64",
                Cpu::Mips => "mips",
                Cpu::Mips64 => "mips64",
                Cpu::Unknown(_) => "unknown",
            }
        )
    }
}

impl PointerWidth {
    pub fn size_in_bytes(self) -> Option<u8> {
        match self {
            Self::Bits32 => Some(4),
            Self::Bits64 => Some(8),
            Self::Unknown => None,
        }
    }
}
