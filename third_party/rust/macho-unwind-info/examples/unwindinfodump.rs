use std::{fmt::Display, fs::File, io::Read};

use macho_unwind_info::opcodes::{OpcodeArm64, OpcodeX86, OpcodeX86_64};
use macho_unwind_info::UnwindInfo;
use object::{Architecture, ObjectSection};

fn main() {
    let mut args = std::env::args_os().skip(1);
    if args.len() < 1 {
        eprintln!("Usage: {} <path>", std::env::args().next().unwrap());
        std::process::exit(1);
    }
    let path = args.next().unwrap();

    let mut data = Vec::new();
    let mut file = File::open(path).unwrap();
    file.read_to_end(&mut data).unwrap();
    let data = &data[..];

    let file = object::File::parse(data).expect("Could not parse object file");
    use object::Object;
    let unwind_info_data_section = file
        .section_by_name_bytes(b"__unwind_info")
        .expect("Could not find __unwind_info section");
    let data = unwind_info_data_section.data().unwrap();
    let arch = file.architecture();

    let info = UnwindInfo::parse(data).unwrap();
    let address_range = info.address_range();
    println!(
        "Unwind info for address range 0x{:08x}-0x{:08x}",
        address_range.start, address_range.end
    );
    println!();
    let mut function_iter = info.functions();
    while let Some(function) = function_iter.next().unwrap() {
        print_entry(function.start_address, function.opcode, arch);
    }
}

fn print_entry(address: u32, opcode: u32, arch: Architecture) {
    match arch {
        Architecture::I386 => print_entry_impl(address, OpcodeX86::parse(opcode)),
        Architecture::X86_64 => print_entry_impl(address, OpcodeX86_64::parse(opcode)),
        Architecture::Aarch64 => print_entry_impl(address, OpcodeArm64::parse(opcode)),
        _ => {}
    }
}

fn print_entry_impl(address: u32, opcode: impl Display) {
    println!("0x{:08x}: {}", address, opcode);
}
