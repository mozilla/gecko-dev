use crate::{
    dir_section::FileWriterError, maps_reader::MappingInfo, mem_writer::MemoryWriterError, Pid,
};
use goblin;
use nix::errno::Errno;
use std::ffi::OsString;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum InitError {
    #[error("failed to read auxv")]
    ReadAuxvFailed(crate::auxv::AuxvError),
    #[error("IO error for file {0}")]
    IOError(String, #[source] std::io::Error),
    #[error("crash thread does not reference principal mapping")]
    PrincipalMappingNotReferenced,
    #[error("Failed Android specific late init")]
    AndroidLateInitError(#[from] AndroidError),
    #[error("Failed to read the page size")]
    PageSizeError(#[from] Errno),
    #[error("Ptrace does not function within the same process")]
    CannotPtraceSameProcess,
}

#[derive(Error, Debug)]
pub enum MapsReaderError {
    #[error("Couldn't parse as ELF file")]
    ELFParsingFailed(#[from] goblin::error::Error),
    #[error("No soname found (filename: {})", .0.to_string_lossy())]
    NoSoName(OsString, #[source] ModuleReaderError),

    // parse_from_line()
    #[error("Map entry malformed: No {0} found")]
    MapEntryMalformed(&'static str),
    #[error("Couldn't parse address")]
    UnparsableInteger(#[from] std::num::ParseIntError),
    #[error("Linux gate location doesn't fit in the required integer type")]
    LinuxGateNotConvertable(#[from] std::num::TryFromIntError),

    // get_mmap()
    #[error("Not safe to open mapping {}", .0.to_string_lossy())]
    NotSafeToOpenMapping(OsString),
    #[error("IO Error")]
    FileError(#[from] std::io::Error),
    #[error("Mmapped file empty or not an ELF file")]
    MmapSanityCheckFailed,
    #[error("Symlink does not match ({0} vs. {1})")]
    SymlinkError(std::path::PathBuf, std::path::PathBuf),
}

#[derive(Debug, Error)]
pub enum CpuInfoError {
    #[error("IO error for file /proc/cpuinfo")]
    IOError(#[from] std::io::Error),
    #[error("Not all entries of /proc/cpuinfo found!")]
    NotAllProcEntriesFound,
    #[error("Couldn't parse core from file")]
    UnparsableInteger(#[from] std::num::ParseIntError),
    #[error("Couldn't parse cores: {0}")]
    UnparsableCores(String),
}

#[derive(Error, Debug)]
pub enum ThreadInfoError {
    #[error("Index out of bounds: Got {0}, only have {1}")]
    IndexOutOfBounds(usize, usize),
    #[error("Either ppid ({1}) or tgid ({2}) not found in {0}")]
    InvalidPid(String, Pid, Pid),
    #[error("IO error")]
    IOError(#[from] std::io::Error),
    #[error("Couldn't parse address")]
    UnparsableInteger(#[from] std::num::ParseIntError),
    #[error("nix::ptrace() error")]
    PtraceError(#[from] nix::Error),
    #[error("Invalid line in /proc/{0}/status: {1}")]
    InvalidProcStatusFile(Pid, String),
}

#[derive(Debug, Error)]
pub enum AndroidError {
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed slice conversion")]
    TryFromSliceError(#[from] std::array::TryFromSliceError),
    #[error("No Android rel found")]
    NoRelFound,
}

#[derive(Debug, Error)]
#[error("Copy from process {child} failed (source {src}, offset: {offset}, length: {length})")]
pub struct CopyFromProcessError {
    pub child: Pid,
    pub src: usize,
    pub offset: usize,
    pub length: usize,
    pub source: nix::Error,
}

#[derive(Debug, Error)]
pub enum DumperError {
    #[error("Failed to get PAGE_SIZE from system")]
    SysConfError(#[from] nix::Error),
    #[error("wait::waitpid(Pid={0}) failed")]
    WaitPidError(Pid, #[source] nix::Error),
    #[error("nix::ptrace::attach(Pid={0}) failed")]
    PtraceAttachError(Pid, #[source] nix::Error),
    #[error("nix::ptrace::detach(Pid={0}) failed")]
    PtraceDetachError(Pid, #[source] nix::Error),
    #[error(transparent)]
    CopyFromProcessError(#[from] CopyFromProcessError),
    #[error("Skipped thread {0} due to it being part of the seccomp sandbox's trusted code")]
    DetachSkippedThread(Pid),
    #[error("No threads left to suspend out of {0}")]
    SuspendNoThreadsLeft(usize),
    #[error("No mapping for stack pointer found")]
    NoStackPointerMapping,
    #[error("Failed slice conversion")]
    TryFromSliceError(#[from] std::array::TryFromSliceError),
    #[error("Couldn't parse as ELF file")]
    ELFParsingFailed(#[from] goblin::error::Error),
    #[error("Could not read value from module")]
    ModuleReaderError(#[from] ModuleReaderError),
    #[error("Not safe to open mapping: {}", .0.to_string_lossy())]
    NotSafeToOpenMapping(OsString),
    #[error("Failed integer conversion")]
    TryFromIntError(#[from] std::num::TryFromIntError),
    #[error("Maps reader error")]
    MapsReaderError(#[from] MapsReaderError),
}

#[derive(Debug, Error)]
pub enum SectionAppMemoryError {
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error)]
pub enum SectionExceptionStreamError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error)]
pub enum SectionHandleDataStreamError {
    #[error("Failed to access file")]
    IOError(#[from] std::io::Error),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed integer conversion")]
    TryFromIntError(#[from] std::num::TryFromIntError),
}

#[derive(Debug, Error)]
pub enum SectionMappingsError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to get effective path of mapping ({0:?})")]
    GetEffectivePathError(MappingInfo, #[source] MapsReaderError),
}

#[derive(Debug, Error)]
pub enum SectionMemInfoListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to read from procfs")]
    ProcfsError(#[from] procfs_core::ProcError),
}

#[derive(Debug, Error)]
pub enum SectionMemListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error)]
pub enum SectionSystemInfoError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to get CPU Info")]
    CpuInfoError(#[from] CpuInfoError),
}

#[derive(Debug, Error)]
pub enum SectionThreadListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed integer conversion")]
    TryFromIntError(#[from] std::num::TryFromIntError),
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to get thread info")]
    ThreadInfoError(#[from] ThreadInfoError),
    #[error("Failed to write to memory buffer")]
    IOError(#[from] std::io::Error),
}

#[derive(Debug, Error)]
pub enum SectionThreadNamesError {
    #[error("Failed integer conversion")]
    TryFromIntError(#[from] std::num::TryFromIntError),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to write to memory buffer")]
    IOError(#[from] std::io::Error),
}

#[derive(Debug, Error)]
pub enum SectionDsoDebugError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Could not find: {0}")]
    CouldNotFind(&'static str),
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to copy memory from process")]
    FromUTF8Error(#[from] std::string::FromUtf8Error),
}

#[derive(Debug, Error)]
pub enum WriterError {
    #[error("Error during init phase")]
    InitError(#[from] InitError),
    #[error(transparent)]
    DumperError(#[from] DumperError),
    #[error("Failed when writing section AppMemory")]
    SectionAppMemoryError(#[from] SectionAppMemoryError),
    #[error("Failed when writing section ExceptionStream")]
    SectionExceptionStreamError(#[from] SectionExceptionStreamError),
    #[error("Failed when writing section HandleDataStream")]
    SectionHandleDataStreamError(#[from] SectionHandleDataStreamError),
    #[error("Failed when writing section MappingsError")]
    SectionMappingsError(#[from] SectionMappingsError),
    #[error("Failed when writing section MemList")]
    SectionMemListError(#[from] SectionMemListError),
    #[error("Failed when writing section SystemInfo")]
    SectionSystemInfoError(#[from] SectionSystemInfoError),
    #[error("Failed when writing section MemoryInfoList")]
    SectionMemoryInfoListError(#[from] SectionMemInfoListError),
    #[error("Failed when writing section ThreadList")]
    SectionThreadListError(#[from] SectionThreadListError),
    #[error("Failed when writing section ThreadNameList")]
    SectionThreadNamesError(#[from] SectionThreadNamesError),
    #[error("Failed when writing section DsoDebug")]
    SectionDsoDebugError(#[from] SectionDsoDebugError),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to write to file")]
    FileWriterError(#[from] FileWriterError),
    #[error("Failed to get current timestamp when writing header of minidump")]
    SystemTimeError(#[from] std::time::SystemTimeError),
}

#[derive(Debug, Error)]
pub enum ModuleReaderError {
    #[error("failed to read module file ({path}): {error}")]
    MapFile {
        path: std::path::PathBuf,
        #[source]
        error: std::io::Error,
    },
    #[error("failed to read module memory: {length} bytes at {offset}{}: {error}", .start_address.map(|addr| format!(" (start address: {addr})")).unwrap_or_default())]
    ReadModuleMemory {
        offset: u64,
        length: u64,
        start_address: Option<u64>,
        #[source]
        error: nix::Error,
    },
    #[error("failed to parse ELF memory: {0}")]
    Parsing(#[from] goblin::error::Error),
    #[error("no build id notes in program headers")]
    NoProgramHeaderNote,
    #[error("no string table available to locate note sections")]
    NoStrTab,
    #[error("no build id note sections")]
    NoSectionNote,
    #[error("the ELF data contains no program headers")]
    NoProgramHeaders,
    #[error("the ELF data contains no sections")]
    NoSections,
    #[error("the ELF data does not have a .text section from which to generate a build id")]
    NoTextSection,
    #[error(
        "failed to calculate build id\n\
    ... from program headers: {program_headers}\n\
    ... from sections: {section}\n\
    ... from the text section: {section}"
    )]
    NoBuildId {
        program_headers: Box<Self>,
        section: Box<Self>,
        generated: Box<Self>,
    },
    #[error("no dynamic string table section")]
    NoDynStrSection,
    #[error("a string in the strtab did not have a terminating nul byte")]
    StrTabNoNulByte,
    #[error("no SONAME found in dynamic linking information")]
    NoSoNameEntry,
    #[error("no dynamic linking information section")]
    NoDynamicSection,
    #[error(
        "failed to retrieve soname\n\
    ... from program headers: {program_headers}\n\
    ... from sections: {section}"
    )]
    NoSoName {
        program_headers: Box<Self>,
        section: Box<Self>,
    },
}
