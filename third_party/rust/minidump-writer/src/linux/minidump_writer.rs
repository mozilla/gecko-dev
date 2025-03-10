pub use crate::linux::auxv::{AuxvType, DirectAuxvDumpInfo};
use {
    crate::{
        auxv::AuxvDumpInfo,
        dir_section::{DirSection, DumpBuf},
        linux::{
            app_memory::AppMemoryList,
            crash_context::CrashContext,
            dso_debug,
            errors::WriterError,
            maps_reader::{MappingInfo, MappingList},
            ptrace_dumper::PtraceDumper,
            sections::*,
        },
        mem_writer::{Buffer, MemoryArrayWriter, MemoryWriter, MemoryWriterError},
        minidump_format::*,
        Pid,
    },
    error_graph::{ErrorList, WriteErrorList},
    std::{
        io::{Seek, Write},
        time::Duration,
    },
};

pub enum CrashingThreadContext {
    None,
    CrashContext(MDLocationDescriptor),
    CrashContextPlusAddress((MDLocationDescriptor, usize)),
}

/// The default timeout after a `SIGSTOP` after which minidump writing proceeds
/// regardless of the process state
pub const STOP_TIMEOUT: Duration = Duration::from_millis(100);

pub struct MinidumpWriter {
    pub process_id: Pid,
    pub blamed_thread: Pid,
    pub minidump_size_limit: Option<u64>,
    pub skip_stacks_if_mapping_unreferenced: bool,
    pub principal_mapping_address: Option<usize>,
    pub user_mapping_list: MappingList,
    pub app_memory: AppMemoryList,
    pub memory_blocks: Vec<MDMemoryDescriptor>,
    pub principal_mapping: Option<MappingInfo>,
    pub sanitize_stack: bool,
    pub crash_context: Option<CrashContext>,
    pub crashing_thread_context: CrashingThreadContext,
    pub stop_timeout: Duration,
    pub direct_auxv_dump_info: Option<DirectAuxvDumpInfo>,
}

// This doesn't work yet:
// https://github.com/rust-lang/rust/issues/43408
// fn write<T: Sized, P: AsRef<Path>>(path: P, value: T) -> Result<()> {
//     let mut file = std::fs::File::open(path)?;
//     let bytes: [u8; size_of::<T>()] = unsafe { transmute(value) };
//     file.write_all(&bytes)?;
//     Ok(())
// }

type Result<T> = std::result::Result<T, WriterError>;

impl MinidumpWriter {
    pub fn new(process: Pid, blamed_thread: Pid) -> Self {
        Self {
            process_id: process,
            blamed_thread,
            minidump_size_limit: None,
            skip_stacks_if_mapping_unreferenced: false,
            principal_mapping_address: None,
            user_mapping_list: MappingList::new(),
            app_memory: AppMemoryList::new(),
            memory_blocks: Vec::new(),
            principal_mapping: None,
            sanitize_stack: false,
            crash_context: None,
            crashing_thread_context: CrashingThreadContext::None,
            stop_timeout: STOP_TIMEOUT,
            direct_auxv_dump_info: None,
        }
    }

    pub fn set_minidump_size_limit(&mut self, limit: u64) -> &mut Self {
        self.minidump_size_limit = Some(limit);
        self
    }

    pub fn set_user_mapping_list(&mut self, user_mapping_list: MappingList) -> &mut Self {
        self.user_mapping_list = user_mapping_list;
        self
    }

    pub fn set_principal_mapping_address(&mut self, principal_mapping_address: usize) -> &mut Self {
        self.principal_mapping_address = Some(principal_mapping_address);
        self
    }

    pub fn set_app_memory(&mut self, app_memory: AppMemoryList) -> &mut Self {
        self.app_memory = app_memory;
        self
    }

    pub fn set_crash_context(&mut self, crash_context: CrashContext) -> &mut Self {
        self.crash_context = Some(crash_context);
        self
    }

    pub fn skip_stacks_if_mapping_unreferenced(&mut self) -> &mut Self {
        self.skip_stacks_if_mapping_unreferenced = true; // Off by default
        self
    }

    pub fn sanitize_stack(&mut self) -> &mut Self {
        self.sanitize_stack = true; // Off by default
        self
    }

    /// Sets the timeout after `SIGSTOP` is sent to the process, if the process
    /// has not stopped by the time the timeout has reached, we proceed with
    /// minidump generation
    pub fn stop_timeout(&mut self, duration: Duration) -> &mut Self {
        self.stop_timeout = duration;
        self
    }

    /// Directly set important Auxv info determined by the crashing process
    ///
    /// Since `/proc/{pid}/auxv` can sometimes be inaccessible, the calling process should prefer to transfer this
    /// information directly using the Linux `getauxval()` call (if possible).
    ///
    /// Any field that is set to `0` will be considered unset. In that case, minidump-writer might try other techniques
    /// to obtain it (like reading `/proc/{pid}/auxv`).
    pub fn set_direct_auxv_dump_info(
        &mut self,
        direct_auxv_dump_info: DirectAuxvDumpInfo,
    ) -> &mut Self {
        self.direct_auxv_dump_info = Some(direct_auxv_dump_info);
        self
    }

    /// Generates a minidump and writes to the destination provided. Returns the in-memory
    /// version of the minidump as well.
    pub fn dump(&mut self, destination: &mut (impl Write + Seek)) -> Result<Vec<u8>> {
        let auxv = self
            .direct_auxv_dump_info
            .clone()
            .map(AuxvDumpInfo::from)
            .unwrap_or_default();

        let mut soft_errors = ErrorList::default();

        let mut dumper = PtraceDumper::new_report_soft_errors(
            self.process_id,
            self.stop_timeout,
            auxv,
            soft_errors.subwriter(WriterError::InitErrors),
        )?;

        let threads_count = dumper.threads.len();

        dumper.suspend_threads(soft_errors.subwriter(WriterError::SuspendThreadsErrors));

        if dumper.threads.is_empty() {
            soft_errors.push(WriterError::SuspendNoThreadsLeft(threads_count));
        }

        dumper.late_init()?;

        if self.skip_stacks_if_mapping_unreferenced {
            if let Some(address) = self.principal_mapping_address {
                self.principal_mapping = dumper.find_mapping_no_bias(address).cloned();
            }

            if !self.crash_thread_references_principal_mapping(&dumper) {
                soft_errors.push(WriterError::PrincipalMappingNotReferenced);
            }
        }

        let mut buffer = Buffer::with_capacity(0);
        self.generate_dump(&mut buffer, &mut dumper, soft_errors, destination)?;

        Ok(buffer.into())
    }

    fn crash_thread_references_principal_mapping(&self, dumper: &PtraceDumper) -> bool {
        if self.crash_context.is_none() || self.principal_mapping.is_none() {
            return false;
        }

        let low_addr = self
            .principal_mapping
            .as_ref()
            .unwrap()
            .system_mapping_info
            .start_address;
        let high_addr = self
            .principal_mapping
            .as_ref()
            .unwrap()
            .system_mapping_info
            .end_address;

        let pc = self
            .crash_context
            .as_ref()
            .unwrap()
            .get_instruction_pointer();
        let stack_pointer = self.crash_context.as_ref().unwrap().get_stack_pointer();

        if pc >= low_addr && pc < high_addr {
            return true;
        }

        let (valid_stack_pointer, stack_len) = match dumper.get_stack_info(stack_pointer) {
            Ok(x) => x,
            Err(_) => {
                return false;
            }
        };

        let stack_copy = match PtraceDumper::copy_from_process(
            self.blamed_thread,
            valid_stack_pointer,
            stack_len,
        ) {
            Ok(x) => x,
            Err(_) => {
                return false;
            }
        };

        let sp_offset = stack_pointer.saturating_sub(valid_stack_pointer);
        self.principal_mapping
            .as_ref()
            .unwrap()
            .stack_has_pointer_to_mapping(&stack_copy, sp_offset)
    }

    fn generate_dump(
        &mut self,
        buffer: &mut DumpBuf,
        dumper: &mut PtraceDumper,
        mut soft_errors: ErrorList<WriterError>,
        destination: &mut (impl Write + Seek),
    ) -> Result<()> {
        // A minidump file contains a number of tagged streams. This is the number
        // of streams which we write.
        let num_writers = 18u32;

        let mut header_section = MemoryWriter::<MDRawHeader>::alloc(buffer)?;

        let mut dir_section = DirSection::new(buffer, num_writers, destination)?;

        let header = MDRawHeader {
            signature: MD_HEADER_SIGNATURE,
            version: MD_HEADER_VERSION,
            stream_count: num_writers,
            //   header.get()->stream_directory_rva = dir.position();
            stream_directory_rva: dir_section.position(),
            checksum: 0, /* Can be 0.  In fact, that's all that's
                          * been found in minidump files. */
            time_date_stamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)?
                .as_secs() as u32, // TODO: This is not Y2038 safe, but thats how its currently defined as
            flags: 0,
        };
        header_section.set_value(buffer, header)?;

        // Ensure the header gets flushed. If we crash somewhere below,
        // we should have a mostly-intact dump
        dir_section.write_to_file(buffer, None)?;

        let dirent = thread_list_stream::write(self, buffer, dumper)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = mappings::write(self, buffer, dumper)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        app_memory::write(self, buffer)?;
        dir_section.write_to_file(buffer, None)?;

        let dirent = memory_list_stream::write(self, buffer)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = exception_stream::write(self, buffer)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = systeminfo_stream::write(
            buffer,
            soft_errors.subwriter(WriterError::WriteSystemInfoErrors),
        )?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = memory_info_list_stream::write(self, buffer)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, "/proc/cpuinfo") {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxCpuInfo as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteCpuInfoFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/status", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxProcStatus as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteThreadProcStatusFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self
            .write_file(buffer, "/etc/lsb-release")
            .or_else(|_| self.write_file(buffer, "/etc/os-release"))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxLsbRelease as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteOsReleaseInfoFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/cmdline", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxCmdLine as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteCommandLineFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/environ", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxEnviron as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteEnvironmentFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/auxv", self.blamed_thread)) {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxAuxv as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteAuxvFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/maps", self.blamed_thread)) {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxMaps as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteMapsFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match dso_debug::write_dso_debug_stream(buffer, self.process_id, &dumper.auxv)
        {
            Ok(dirent) => dirent,
            Err(e) => {
                soft_errors.push(WriterError::WriteDSODebugStreamFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/limits", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::MozLinuxLimits as u32,
                location,
            },
            Err(e) => {
                soft_errors.push(WriterError::WriteLimitsFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = thread_names_stream::write(buffer, dumper)?;
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match handle_data_stream::write(self, buffer) {
            Ok(dirent) => dirent,
            Err(e) => {
                soft_errors.push(WriterError::WriteHandleDataStreamFailed(e));
                Default::default()
            }
        };
        dir_section.write_to_file(buffer, Some(dirent))?;

        // ========================================================================================
        //
        // PAST THIS BANNER, THE THREADS ARE RUNNING IN THE TARGET PROCESS AGAIN. IF YOU NEED TO
        // ADD NEW ENTRIES THAT ACCESS THE TARGET MEMORY, DO IT BEFORE HERE!
        //
        // ========================================================================================

        // Collect any last-minute soft errors when trying to restart threads
        dumper.resume_threads(soft_errors.subwriter(WriterError::ResumeThreadsErrors));

        // If this fails, there's really nothing we can do about that (other than ignore it).
        let dirent = write_soft_errors(buffer, soft_errors)
            .map(|location| MDRawDirectory {
                stream_type: MDStreamType::MozSoftErrors as u32,
                location,
            })
            .unwrap_or_default();
        dir_section.write_to_file(buffer, Some(dirent))?;

        // If you add more directory entries, don't forget to update num_writers, above.
        Ok(())
    }

    #[allow(clippy::unused_self)]
    fn write_file(
        &self,
        buffer: &mut DumpBuf,
        filename: &str,
    ) -> std::result::Result<MDLocationDescriptor, MemoryWriterError> {
        let content = std::fs::read(filename)?;

        let section = MemoryArrayWriter::write_bytes(buffer, &content);
        Ok(section.location())
    }
}

fn write_soft_errors(
    buffer: &mut DumpBuf,
    soft_errors: ErrorList<WriterError>,
) -> Result<MDLocationDescriptor> {
    let soft_errors_json_str =
        serde_json::to_string_pretty(&soft_errors).map_err(WriterError::ConvertToJsonFailed)?;
    let section = MemoryArrayWriter::write_bytes(buffer, soft_errors_json_str.as_bytes());
    Ok(section.location())
}
