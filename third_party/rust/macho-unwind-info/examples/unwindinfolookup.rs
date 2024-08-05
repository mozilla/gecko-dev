use std::{fmt::Display, fs::File, io::Read};

use macho_unwind_info::opcodes::{OpcodeArm64, OpcodeX86, OpcodeX86_64};
use macho_unwind_info::UnwindInfo;
use object::{Architecture, ObjectSection};

fn main() {
    let mut args = std::env::args().skip(1);
    if args.len() < 1 {
        eprintln!("Usage: {} <path> <pc>", std::env::args().next().unwrap());
        std::process::exit(1);
    }
    let path = args.next().unwrap();
    let pc = args.next().unwrap();
    let pc: u32 = if let Some(hexstr) = pc.strip_prefix("0x") {
        u32::from_str_radix(hexstr, 16).unwrap()
    } else {
        pc.parse().unwrap()
    };

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

    let unwind_info = UnwindInfo::parse(data).unwrap();
    let function = match unwind_info.lookup(pc) {
        Ok(Some(f)) => f,
        Ok(None) => {
            println!("No entry was found for address 0x{:x}", pc);
            std::process::exit(1);
        }
        Err(e) => {
            println!(
                "There was an error when looking up address 0x{:x}: {}",
                pc, e
            );
            std::process::exit(1);
        }
    };
    print_entry(function.start_address, function.opcode, arch);
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
    println!(
        "Found entry with function address 0x{:08x} and opcode {}",
        address, opcode
    );
}
