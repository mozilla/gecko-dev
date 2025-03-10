use {
    super::{ptrace_dumper::InitError, serializers::*},
    crate::{
        dir_section::FileWriterError, maps_reader::MappingInfo, mem_writer::MemoryWriterError,
        serializers::*, Pid,
    },
    error_graph::ErrorList,
    std::ffi::OsString,
    thiserror::Error,
};

#[derive(Error, Debug, serde::Serialize)]
pub enum MapsReaderError {
    #[error("Couldn't parse as ELF file")]
    ELFParsingFailed(
        #[from]
        #[serde(serialize_with = "serialize_goblin_error")]
        goblin::error::Error,
    ),
    #[error("No soname found (filename: {})", .0.to_string_lossy())]
    NoSoName(OsString, #[source] ModuleReaderError),

    // parse_from_line()
    #[error("Map entry malformed: No {0} found")]
    MapEntryMalformed(&'static str),
    #[error("Couldn't parse address")]
    UnparsableInteger(
        #[from]
        #[serde(skip)]
        std::num::ParseIntError,
    ),
    #[error("Linux gate location doesn't fit in the required integer type")]
    LinuxGateNotConvertable(
        #[from]
        #[serde(skip)]
        std::num::TryFromIntError,
    ),

    // get_mmap()
    #[error("Not safe to open mapping {}", .0.to_string_lossy())]
    NotSafeToOpenMapping(OsString),
    #[error("IO Error")]
    FileError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
    #[error("Mmapped file empty or not an ELF file")]
    MmapSanityCheckFailed,
    #[error("Symlink does not match ({0} vs. {1})")]
    SymlinkError(std::path::PathBuf, std::path::PathBuf),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum CpuInfoError {
    #[error("IO error for file /proc/cpuinfo")]
    IOError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
    #[error("Not all entries of /proc/cpuinfo found!")]
    NotAllProcEntriesFound,
    #[error("Couldn't parse core from file")]
    UnparsableInteger(
        #[from]
        #[serde(skip)]
        std::num::ParseIntError,
    ),
    #[error("Couldn't parse cores: {0}")]
    UnparsableCores(String),
}

#[derive(Error, Debug, serde::Serialize)]
pub enum ThreadInfoError {
    #[error("Index out of bounds: Got {0}, only have {1}")]
    IndexOutOfBounds(usize, usize),
    #[error("Either ppid ({1}) or tgid ({2}) not found in {0}")]
    InvalidPid(String, Pid, Pid),
    #[error("IO error")]
    IOError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
    #[error("Couldn't parse address")]
    UnparsableInteger(
        #[from]
        #[serde(skip)]
        std::num::ParseIntError,
    ),
    #[error("nix::ptrace() error")]
    PtraceError(
        #[from]
        #[serde(serialize_with = "serialize_nix_error")]
        nix::Error,
    ),
    #[error("Invalid line in /proc/{0}/status: {1}")]
    InvalidProcStatusFile(Pid, String),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum AndroidError {
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed slice conversion")]
    TryFromSliceError(
        #[from]
        #[serde(skip)]
        std::array::TryFromSliceError,
    ),
    #[error("No Android rel found")]
    NoRelFound,
}

#[derive(Debug, Error, serde::Serialize)]
#[error("Copy from process {child} failed (source {src}, offset: {offset}, length: {length})")]
pub struct CopyFromProcessError {
    pub child: Pid,
    pub src: usize,
    pub offset: usize,
    pub length: usize,
    #[serde(serialize_with = "serialize_nix_error")]
    pub source: nix::Error,
}

#[derive(Debug, Error, serde::Serialize)]
pub enum DumperError {
    #[error("Failed to get PAGE_SIZE from system")]
    SysConfError(
        #[from]
        #[serde(serialize_with = "serialize_nix_error")]
        nix::Error,
    ),
    #[error("wait::waitpid(Pid={0}) failed")]
    WaitPidError(
        Pid,
        #[source]
        #[serde(serialize_with = "serialize_nix_error")]
        nix::Error,
    ),
    #[error("nix::ptrace::attach(Pid={0}) failed")]
    PtraceAttachError(
        Pid,
        #[source]
        #[serde(serialize_with = "serialize_nix_error")]
        nix::Error,
    ),
    #[error("nix::ptrace::detach(Pid={0}) failed")]
    PtraceDetachError(
        Pid,
        #[source]
        #[serde(serialize_with = "serialize_nix_error")]
        nix::Error,
    ),
    #[error(transparent)]
    CopyFromProcessError(#[from] CopyFromProcessError),
    #[error("Skipped thread {0} due to it being part of the seccomp sandbox's trusted code")]
    DetachSkippedThread(Pid),
    #[error("No mapping for stack pointer found")]
    NoStackPointerMapping,
    #[error("Failed slice conversion")]
    TryFromSliceError(
        #[from]
        #[serde(skip)]
        std::array::TryFromSliceError,
    ),
    #[error("Couldn't parse as ELF file")]
    ELFParsingFailed(
        #[from]
        #[serde(serialize_with = "serialize_goblin_error")]
        goblin::error::Error,
    ),
    #[error("Could not read value from module")]
    ModuleReaderError(#[from] ModuleReaderError),
    #[error("Not safe to open mapping: {}", .0.to_string_lossy())]
    NotSafeToOpenMapping(OsString),
    #[error("Failed integer conversion")]
    TryFromIntError(
        #[from]
        #[serde(skip)]
        std::num::TryFromIntError,
    ),
    #[error("Maps reader error")]
    MapsReaderError(#[from] MapsReaderError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionAppMemoryError {
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionExceptionStreamError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionHandleDataStreamError {
    #[error("Failed to access file")]
    IOError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed integer conversion")]
    TryFromIntError(
        #[from]
        #[serde(skip)]
        std::num::TryFromIntError,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionMappingsError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to get effective path of mapping ({0:?})")]
    GetEffectivePathError(MappingInfo, #[source] MapsReaderError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionMemInfoListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to read from procfs")]
    ProcfsError(
        #[from]
        #[serde(serialize_with = "serialize_proc_error")]
        procfs_core::ProcError,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionMemListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionSystemInfoError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to get CPU Info")]
    CpuInfoError(#[from] CpuInfoError),
    #[error("Failed trying to write CPU information")]
    WriteCpuInformationFailed(#[source] CpuInfoError),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionThreadListError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed integer conversion")]
    TryFromIntError(
        #[from]
        #[serde(skip)]
        std::num::TryFromIntError,
    ),
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to get thread info")]
    ThreadInfoError(#[from] ThreadInfoError),
    #[error("Failed to write to memory buffer")]
    IOError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionThreadNamesError {
    #[error("Failed integer conversion")]
    TryFromIntError(
        #[from]
        #[serde(skip)]
        std::num::TryFromIntError,
    ),
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Failed to write to memory buffer")]
    IOError(
        #[from]
        #[serde(serialize_with = "serialize_io_error")]
        std::io::Error,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum SectionDsoDebugError {
    #[error("Failed to write to memory")]
    MemoryWriterError(#[from] MemoryWriterError),
    #[error("Could not find: {0}")]
    CouldNotFind(&'static str),
    #[error("Failed to copy memory from process")]
    CopyFromProcessError(#[from] DumperError),
    #[error("Failed to copy memory from process")]
    FromUTF8Error(
        #[from]
        #[serde(serialize_with = "serialize_from_utf8_error")]
        std::string::FromUtf8Error,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
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
    SystemTimeError(
        #[from]
        #[serde(serialize_with = "serialize_system_time_error")]
        std::time::SystemTimeError,
    ),
    #[error("Errors occurred while initializing PTraceDumper")]
    InitErrors(#[source] ErrorList<InitError>),
    #[error("Errors occurred while suspending threads")]
    SuspendThreadsErrors(#[source] ErrorList<DumperError>),
    #[error("Errors occurred while resuming threads")]
    ResumeThreadsErrors(#[source] ErrorList<DumperError>),
    #[error("Crash thread does not reference principal mapping")]
    PrincipalMappingNotReferenced,
    #[error("Errors occurred while writing system info")]
    WriteSystemInfoErrors(#[source] ErrorList<SectionSystemInfoError>),
    #[error("Failed writing cpuinfo")]
    WriteCpuInfoFailed(#[source] MemoryWriterError),
    #[error("Failed writing thread proc status")]
    WriteThreadProcStatusFailed(#[source] MemoryWriterError),
    #[error("Failed writing OS Release Information")]
    WriteOsReleaseInfoFailed(#[source] MemoryWriterError),
    #[error("Failed writing process command line")]
    WriteCommandLineFailed(#[source] MemoryWriterError),
    #[error("Writing process environment failed")]
    WriteEnvironmentFailed(#[source] MemoryWriterError),
    #[error("Failed to write auxv file")]
    WriteAuxvFailed(#[source] MemoryWriterError),
    #[error("Failed to write maps file")]
    WriteMapsFailed(#[source] MemoryWriterError),
    #[error("Failed writing DSO Debug Stream")]
    WriteDSODebugStreamFailed(#[source] SectionDsoDebugError),
    #[error("Failed writing limits file")]
    WriteLimitsFailed(#[source] MemoryWriterError),
    #[error("Failed writing handle data stream")]
    WriteHandleDataStreamFailed(#[source] SectionHandleDataStreamError),
    #[error("Failed writing handle data stream direction entry")]
    WriteHandleDataStreamDirentFailed(#[source] FileWriterError),
    #[error("No threads left to suspend out of {0}")]
    SuspendNoThreadsLeft(usize),
    #[error("Failed to convert soft error list to JSON")]
    ConvertToJsonFailed(
        #[source]
        #[serde(skip)]
        serde_json::Error,
    ),
}

#[derive(Debug, Error, serde::Serialize)]
pub enum ModuleReaderError {
    #[error("failed to read module file ({path}): {error}")]
    MapFile {
        path: std::path::PathBuf,
        #[source]
        #[serde(serialize_with = "serialize_io_error")]
        error: std::io::Error,
    },
    #[error("failed to read module memory: {length} bytes at {offset}{}: {error}", .start_address.map(|addr| format!(" (start address: {addr})")).unwrap_or_default())]
    ReadModuleMemory {
        offset: u64,
        length: u64,
        start_address: Option<u64>,
        #[source]
        #[serde(serialize_with = "serialize_nix_error")]
        error: nix::Error,
    },
    #[error("failed to parse ELF memory: {0}")]
    Parsing(
        #[from]
        #[serde(serialize_with = "serialize_goblin_error")]
        goblin::error::Error,
    ),
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
