// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use debugid::{CodeId, DebugId};
use minidump::system_info::{Cpu, Os};
use minidump::*;
use minidump_common::format as md;
use num_traits::cast::FromPrimitive;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;
use std::time::SystemTime;

fn get_test_minidump_path(filename: &str) -> PathBuf {
    let mut path = PathBuf::from(file!());
    path.pop();
    path.pop();
    path.pop();
    path.push("../");
    path.push("testdata");
    path.push(filename);
    println!("{path:?}");
    path
}

fn read_test_minidump() -> Result<MmapMinidump, Error> {
    let path = get_test_minidump_path("test.dmp");
    Minidump::read_path(path)
}

fn read_linux_minidump() -> Result<MmapMinidump, Error> {
    let path = get_test_minidump_path("linux-mini.dmp");
    Minidump::read_path(path)
}

#[ctor::ctor]
fn init_logger() {
    env_logger::builder().is_test(true).init();
}

#[test]
fn test_minidump_read_path() {
    read_test_minidump().unwrap();
}

#[test]
fn test_minidump_read() {
    let path = get_test_minidump_path("test.dmp");
    let mut f = File::open(path).unwrap();
    let mut buf = vec![];
    f.read_to_end(&mut buf).unwrap();
    let _dump = Minidump::read(buf).unwrap();
}

#[test]
fn test_module_list() {
    let dump = read_test_minidump().unwrap();
    let module_list = dump.get_stream::<MinidumpModuleList>().unwrap();
    assert_eq!(
        module_list.module_at_address(0x400000).unwrap().code_file(),
        "c:\\test_app.exe"
    );
    let modules = module_list.iter().collect::<Vec<_>>();
    let module_files = modules.iter().map(|m| m.code_file()).collect::<Vec<_>>();
    assert_eq!(modules.len(), 13);
    assert_eq!(modules[0].base_address(), 0x400000);
    assert_eq!(modules[0].size(), 0x2d000);
    assert_eq!(modules[0].code_file(), "c:\\test_app.exe");
    assert_eq!(
        modules[0].code_identifier().unwrap(),
        CodeId::new("45D35F6C2d000".to_string())
    );
    assert_eq!(modules[0].debug_file().unwrap(), "c:\\test_app.pdb");
    assert_eq!(
        modules[0].debug_identifier().unwrap(),
        DebugId::from_breakpad("5A9832E5287241C1838ED98914E9B7FF1").unwrap()
    );
    assert!(modules[0].version().is_none());

    assert_eq!(modules[12].base_address(), 0x76bf0000);
    assert_eq!(modules[12].size(), 0xb000);
    assert_eq!(modules[12].code_file(), "C:\\WINDOWS\\system32\\psapi.dll");
    assert_eq!(
        modules[12].code_identifier().unwrap(),
        CodeId::new("411096CAb000".to_string())
    );
    assert_eq!(modules[12].debug_file().unwrap(), "psapi.pdb");
    assert_eq!(
        modules[12].debug_identifier().unwrap(),
        DebugId::from_breakpad("A5C3A1F9689F43D8AD228A09293889702").unwrap()
    );
    assert_eq!(modules[12].version().unwrap(), "5.1.2600.2180");

    assert_eq!(
        module_files,
        vec![
            r"c:\test_app.exe",
            r"C:\WINDOWS\system32\ntdll.dll",
            r"C:\WINDOWS\system32\kernel32.dll",
            r"C:\WINDOWS\system32\ole32.dll",
            r"C:\WINDOWS\system32\advapi32.dll",
            r"C:\WINDOWS\system32\rpcrt4.dll",
            r"C:\WINDOWS\system32\gdi32.dll",
            r"C:\WINDOWS\system32\user32.dll",
            r"C:\WINDOWS\system32\msvcrt.dll",
            r"C:\WINDOWS\system32\imm32.dll",
            r"C:\WINDOWS\system32\dbghelp.dll",
            r"C:\WINDOWS\system32\version.dll",
            r"C:\WINDOWS\system32\psapi.dll",
        ]
    );

    assert_eq!(
        module_list
            .by_addr()
            .map(|m| m.code_file())
            .collect::<Vec<_>>(),
        vec![
            r"c:\test_app.exe",
            r"C:\WINDOWS\system32\dbghelp.dll",
            r"C:\WINDOWS\system32\imm32.dll",
            r"C:\WINDOWS\system32\psapi.dll",
            r"C:\WINDOWS\system32\ole32.dll",
            r"C:\WINDOWS\system32\version.dll",
            r"C:\WINDOWS\system32\msvcrt.dll",
            r"C:\WINDOWS\system32\user32.dll",
            r"C:\WINDOWS\system32\advapi32.dll",
            r"C:\WINDOWS\system32\rpcrt4.dll",
            r"C:\WINDOWS\system32\gdi32.dll",
            r"C:\WINDOWS\system32\kernel32.dll",
            r"C:\WINDOWS\system32\ntdll.dll",
        ]
    );
}

#[test]
fn test_system_info() {
    let dump = read_test_minidump().unwrap();
    let system_info = dump.get_stream::<MinidumpSystemInfo>().unwrap();
    assert_eq!(system_info.os, Os::Windows);
    assert_eq!(system_info.cpu, Cpu::X86);
    assert_eq!(
        system_info.cpu_info().unwrap(),
        "GenuineIntel family 6 model 13 stepping 8"
    );
    assert_eq!(&system_info.csd_version().unwrap(), "Service Pack 2");
}

#[test]
fn test_misc_info() {
    let dump = read_test_minidump().unwrap();
    let misc_info = dump.get_stream::<MinidumpMiscInfo>().unwrap();
    assert_eq!(misc_info.raw.process_id(), Some(&3932));
    assert_eq!(misc_info.raw.process_create_time(), Some(&0x45d35f73));
    assert_eq!(
        misc_info
            .process_create_time()
            .unwrap()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap()
            .as_secs(),
        1171480435, // = 2007-02-14T19:13:55
    );
}

#[test]
fn test_breakpad_info() {
    let dump = read_test_minidump().unwrap();
    let breakpad_info = dump.get_stream::<MinidumpBreakpadInfo>().unwrap();
    assert_eq!(breakpad_info.dump_thread_id.unwrap(), 0x11c0);
    assert_eq!(breakpad_info.requesting_thread_id.unwrap(), 0xbf4);
}

#[test]
fn test_crashpad_info() {
    let path = get_test_minidump_path("simple-crashpad.dmp");
    let dump = Minidump::read_path(&path).unwrap();
    let crashpad_info = dump.get_stream::<MinidumpCrashpadInfo>().unwrap();

    let report_id = md::GUID {
        data1: 0x42F9_DE72,
        data2: 0x518A,
        data3: 0x43DD,
        data4: [0x97, 0xD7, 0x8D, 0xDC, 0x32, 0x8D, 0x36, 0x62],
    };
    assert_eq!(crashpad_info.raw.report_id, report_id);

    let client_id = md::GUID {
        data1: 0x6FD2_B3B9,
        data2: 0x9833,
        data3: 0x4B2F,
        data4: [0xBB, 0xF7, 0xB, 0xCF, 0x50, 0x1B, 0xAD, 0x7E],
    };
    assert_eq!(crashpad_info.raw.client_id, client_id);

    assert_eq!(crashpad_info.simple_annotations["hello"], "world");
    assert_eq!(crashpad_info.module_list.len(), 2);

    let module = &crashpad_info.module_list[0];
    assert_eq!(module.module_index, 16);
    assert_eq!(module.list_annotations, vec!["abort() called".to_owned()]);
    assert!(module.simple_annotations.is_empty());
    assert!(module.annotation_objects.is_empty());
}

#[test]
fn test_assertion() {
    let path = get_test_minidump_path("invalid-parameter.dmp");
    let dump = Minidump::read_path(&path).unwrap();
    let assertion = dump.get_stream::<MinidumpAssertion>().unwrap();
    assert_eq!(assertion.expression().unwrap(), "format != nullptr");
    assert_eq!(assertion.function().unwrap(), "common_vfprintf");
    assert_eq!(
        assertion.file().unwrap(),
        r"minkernel\crts\ucrt\src\appcrt\stdio\output.cpp"
    );
    assert_eq!(assertion.raw.line, 32);
    assert_eq!(
        md::AssertionType::from_u32(assertion.raw._type),
        Some(md::AssertionType::InvalidParameter)
    );
}

#[test]
fn test_exception() {
    let dump = read_test_minidump().unwrap();
    let exception = dump.get_stream::<MinidumpException>().unwrap();
    let system_info = dump.get_stream::<MinidumpSystemInfo>().unwrap();
    let misc_info = dump.get_stream::<MinidumpMiscInfo>().ok();
    assert_eq!(exception.thread_id, 0xbf4);
    assert_eq!(exception.raw.exception_record.exception_code, 0xc0000005);
    if let Some(ctx) = exception
        .context(&system_info, misc_info.as_ref())
        .as_deref()
    {
        assert_eq!(ctx.get_instruction_pointer(), 0x40429e);
        assert_eq!(ctx.get_stack_pointer(), 0x12fe84);
        if let MinidumpContext {
            raw: MinidumpRawContext::X86(ref raw),
            ref valid,
        } = *ctx
        {
            assert_eq!(raw.eip, 0x40429e);
            assert_eq!(*valid, MinidumpContextValidity::All);
        } else {
            panic!("Wrong context type");
        }
    } else {
        panic!("Missing context");
    }
}

#[test]
fn test_thread_list() {
    let dump = read_test_minidump().unwrap();
    let thread_list = dump.get_stream::<MinidumpThreadList<'_>>().unwrap();
    let system_info = dump.get_stream::<MinidumpSystemInfo>().unwrap();
    let misc_info = dump.get_stream::<MinidumpMiscInfo>().ok();
    let memory_list = dump.get_memory().unwrap_or_default();

    let threads = &thread_list.threads;
    assert_eq!(threads.len(), 2);
    assert_eq!(threads[0].raw.thread_id, 0xbf4);
    assert_eq!(threads[1].raw.thread_id, 0x11c0);
    let id = threads[1].raw.thread_id;
    assert_eq!(thread_list.get_thread(id).unwrap().raw.thread_id, id);
    if let Some(ctx) = threads[0]
        .context(&system_info, misc_info.as_ref())
        .as_deref()
    {
        assert_eq!(ctx.get_instruction_pointer(), 0x7c90eb94);
        assert_eq!(ctx.get_stack_pointer(), 0x12f320);
        if let MinidumpContext {
            raw: MinidumpRawContext::X86(ref raw),
            ref valid,
        } = *ctx
        {
            assert_eq!(raw.eip, 0x7c90eb94);
            assert_eq!(*valid, MinidumpContextValidity::All);
        } else {
            panic!("Wrong context type");
        }
    } else {
        panic!("Missing context");
    }
    if let Some(ref stack) = threads[0].stack_memory(&memory_list) {
        // Try the beginning
        assert_eq!(stack.get_memory_at_address::<u8>(0x12f31c).unwrap(), 0);
        assert_eq!(stack.get_memory_at_address::<u16>(0x12f31c).unwrap(), 0);
        assert_eq!(stack.get_memory_at_address::<u32>(0x12f31c).unwrap(), 0);
        assert_eq!(
            stack.get_memory_at_address::<u64>(0x12f31c).unwrap(),
            0x7c90e9c000000000
        );
        // And the end
        assert_eq!(stack.get_memory_at_address::<u8>(0x12ffff).unwrap(), 0);
        assert_eq!(stack.get_memory_at_address::<u16>(0x12fffe).unwrap(), 0);
        assert_eq!(stack.get_memory_at_address::<u32>(0x12fffc).unwrap(), 0);
        assert_eq!(
            stack.get_memory_at_address::<u64>(0x12fff8).unwrap(),
            0x405443
        );
    } else {
        panic!("Missing stack memory");
    }
}

#[test]
fn test_empty_minidump() {
    match Minidump::read(&b""[..]) {
        Ok(_) => panic!("Should have failed to read minidump"),
        Err(e) => assert_eq!(e, Error::MissingHeader),
    }
}

#[test]
fn backwards_range() {
    let data = include_bytes!("../../testdata/invalid-range.dmp");

    match Minidump::read(&data[..]) {
        Ok(f) => {
            // TODO verify this is correct
            // This seems to call `MinidumpStream::read()` with a `bytes` that is the entire
            // minidump!
            let _ = f
                .get_stream::<MinidumpLinuxMaps>()
                .expect_err("range should be invalid");
        }
        Err(e) => {
            panic!("Expected to parse the header, got {:?}", e);
        }
    }
}

#[test]
fn test_record_count_mac_info() {
    let data = include_bytes!("../../testdata/invalid-record-count.dmp");

    match Minidump::read(&data[..]) {
        Ok(f) => {
            let _ = f.get_stream::<MinidumpMacCrashInfo>();
        }
        Err(e) => {
            panic!("Expected to parse the header, got {:?}", e);
        }
    }
}

#[test]
fn test_linux_os_version() {
    let dump = read_linux_minidump().unwrap();
    let system_info = dump.get_stream::<MinidumpSystemInfo>().unwrap();

    assert_eq!(system_info.os_parts().0, "4.9.60-linuxkit-aufs");
    assert_eq!(
        system_info.os_parts().1.unwrap(),
        "#1 SMP Mon Nov 6 16:00:12 UTC 2017",
    );
}

#[test]
fn test_full_dump_memory() {
    let path = get_test_minidump_path("full-dump.dmp");
    let dump = Minidump::read_path(&path).unwrap();
    let memory_list = dump.get_stream::<MinidumpMemory64List<'_>>().unwrap();
    assert_eq!(memory_list.iter().count(), 54);
    let blocks: Vec<_> = memory_list.iter().take(3).collect();
    assert_eq!(blocks[0].base_address, 0x007FFE0000);
    assert_eq!(blocks[0].size, 0x1000);
    assert_eq!(blocks[0].bytes[0..8], [0, 0, 0, 0, 0, 0, 0xA0, 0x0F]);
    assert_eq!(blocks[1].base_address, 0x007FFE9000);
    assert_eq!(blocks[1].size, 0x1000);
    assert_eq!(blocks[1].bytes[0..8], [0x48, 0x61, 0x6C, 0x54, 0, 0, 0, 0]);
    assert_eq!(blocks[2].base_address, 0x9897D0D000);
    assert_eq!(blocks[2].size, 0x3000);
    assert_eq!(blocks[2].bytes[0..8], [0, 0, 0, 0, 0, 0, 0, 0]);
}
