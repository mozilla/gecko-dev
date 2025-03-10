use {
    super::*, crate::linux::dumper_cpu_info as dci, error_graph::WriteErrorList,
    errors::SectionSystemInfoError,
};

pub fn write(
    buffer: &mut DumpBuf,
    mut soft_errors: impl WriteErrorList<SectionSystemInfoError>,
) -> Result<MDRawDirectory, SectionSystemInfoError> {
    let mut info_section = MemoryWriter::<MDRawSystemInfo>::alloc(buffer)?;
    let dirent = MDRawDirectory {
        stream_type: MDStreamType::SystemInfoStream as u32,
        location: info_section.location(),
    };

    let (platform_id, os_version) = dci::os_information();
    let os_version_loc = write_string_to_location(buffer, &os_version)?;

    // SAFETY: POD
    let mut info = unsafe { std::mem::zeroed::<MDRawSystemInfo>() };
    info.platform_id = platform_id as u32;
    info.csd_version_rva = os_version_loc.rva;

    if let Err(e) = dci::write_cpu_information(&mut info) {
        soft_errors.push(SectionSystemInfoError::WriteCpuInformationFailed(e));
    }

    info_section.set_value(buffer, info)?;
    Ok(dirent)
}
