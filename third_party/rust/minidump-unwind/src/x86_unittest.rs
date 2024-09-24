// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use crate::*;
use minidump::format::CONTEXT_X86;
use minidump::system_info::{Cpu, Os};
use std::collections::HashMap;
use test_assembler::*;

struct TestFixture {
    pub raw: CONTEXT_X86,
    pub modules: MinidumpModuleList,
    pub symbols: HashMap<String, String>,
}

impl TestFixture {
    pub fn new() -> TestFixture {
        TestFixture {
            raw: CONTEXT_X86::default(),
            // Give the two modules reasonable standard locations and names
            // for tests to play with.
            modules: MinidumpModuleList::from_modules(vec![
                MinidumpModule::new(0x40000000, 0x10000, "module1"),
                MinidumpModule::new(0x50000000, 0x10000, "module2"),
            ]),
            symbols: HashMap::new(),
        }
    }

    pub async fn walk_stack(&self, stack: Section) -> CallStack {
        let context = MinidumpContext {
            raw: MinidumpRawContext::X86(self.raw.clone()),
            valid: MinidumpContextValidity::All,
        };
        let base = stack.start().value().unwrap();
        let size = stack.size();
        let stack = stack.get_contents().unwrap();
        let stack_memory = MinidumpMemory {
            desc: Default::default(),
            base_address: base,
            size,
            bytes: &stack,
            endian: scroll::LE,
        };
        let system_info = SystemInfo {
            os: Os::Windows,
            os_version: None,
            os_build: None,
            cpu: Cpu::X86,
            cpu_info: None,
            cpu_microcode_version: None,
            cpu_count: 1,
        };
        let symbolizer = Symbolizer::new(string_symbol_supplier(self.symbols.clone()));
        let mut stack = CallStack::with_context(context);

        walk_stack(
            0,
            (),
            &mut stack,
            Some(UnifiedMemory::Memory(&stack_memory)),
            &self.modules,
            &system_info,
            &symbolizer,
        )
        .await;

        stack
    }

    pub fn add_symbols(&mut self, name: String, symbols: String) {
        self.symbols.insert(name, symbols);
    }
}

#[tokio::test]
async fn test_simple() {
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);
    stack = stack.D32(0).D32(0); // end-of-stack marker
    f.raw.eip = 0x40000200;
    f.raw.ebp = 0x80000000;
    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);
    let f = &s.frames[0];
    let m = f.module.as_ref().unwrap();
    assert_eq!(m.code_file(), "module1");
}

// Walk a traditional frame. A traditional frame saves the caller's
// %ebp just below the return address, and has its own %ebp pointing
// at the saved %ebp.
#[tokio::test]
async fn test_traditional() {
    let mut f = TestFixture::new();
    let frame0_ebp = Label::new();
    let frame1_ebp = Label::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);
    stack = stack
        .append_repeated(12, 0) // frame 0: space
        .mark(&frame0_ebp) // frame 0 %ebp points here
        .D32(&frame1_ebp) // frame 0: saved %ebp
        .D32(0x40008679) // frame 0: resume address
        .append_repeated(8, 0) // frame 1: space
        .mark(&frame1_ebp) // frame 1 %ebp points here
        .D32(0) // frame 1: saved %ebp (stack end)
        .D32(0); // frame 1: return address (stack end)
    f.raw.eip = 0x4000c7a5;
    f.raw.esp = stack.start().value().unwrap() as u32;
    f.raw.ebp = frame0_ebp.value().unwrap() as u32;
    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);
    {
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x4000c7a5);
        assert_eq!(f0.resume_address, 0x4000c7a5);
        // eip
        // ebp
    }
    {
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::FramePointer);
        // ContextValidity
        assert_eq!(f1.instruction, 0x40008678);
        assert_eq!(f1.resume_address, 0x40008679);
        // eip
        // ebp
    }
}

// Walk a traditional frame, but use a bogus %ebp value, forcing a scan
// of the stack for something that looks like a return address.
#[tokio::test]
async fn test_traditional_scan() {
    let mut f = TestFixture::new();
    let frame1_esp = Label::new();
    let frame1_ebp = Label::new();
    let mut stack = Section::new();
    let stack_start = 0x80000000;
    stack.start().set_const(stack_start);
    stack = stack
        // frame 0
        .D32(0xf065dc76) // locals area:
        .D32(0x46ee2167) // garbage that doesn't look like
        .D32(0xbab023ec) // a return address
        .D32(&frame1_ebp) // saved %ebp (%ebp fails to point here, forcing scan)
        .D32(0x4000129d) // return address
        // frame 1
        .mark(&frame1_esp)
        .append_repeated(8, 0) // space
        .mark(&frame1_ebp) // %ebp points here
        .D32(0) // saved %ebp (stack end)
        .D32(0); // return address (stack end)

    f.raw.eip = 0x4000f49d;
    f.raw.esp = stack.start().value().unwrap() as u32;
    // Make the frame pointer bogus, to make the stackwalker scan the stack
    // for something that looks like a return address.
    f.raw.ebp = 0xd43eed6e;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x4000f49d);
        assert_eq!(f0.resume_address, 0x4000f49d);

        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.eip, 0x4000f49d);
            assert_eq!(ctx.esp, stack_start as u32);
            assert_eq!(ctx.ebp, 0xd43eed6e);
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
        } else {
            unreachable!();
        }
        assert_eq!(f1.instruction + 1, 0x4000129d);
        assert_eq!(f1.resume_address, 0x4000129d);

        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, 0x4000129d);
            assert_eq!(ctx.esp, frame1_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame1_ebp.value().unwrap() as u32);
        } else {
            unreachable!();
        }
    }
}

// Force scanning for a return address a long way down the stack
#[tokio::test]
async fn test_traditional_scan_long_way() {
    let mut f = TestFixture::new();
    let frame1_esp = Label::new();
    let frame1_ebp = Label::new();
    let mut stack = Section::new();
    let stack_start = 0x80000000;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0
        .D32(0xf065dc76) // locals area:
        .D32(0x46ee2167) // garbage that doesn't look like
        .D32(0xbab023ec) // a return address
        .append_repeated(20 * 4, 0) // a bunch of space
        .D32(&frame1_ebp) // saved %ebp (%ebp fails to point here, forcing scan)
        .D32(0x4000129d) // return address
        // frame 1
        .mark(&frame1_esp)
        .append_repeated(8, 0) // space
        .mark(&frame1_ebp) // %ebp points here
        .D32(0) // saved %ebp (stack end)
        .D32(0); // return address (stack end)

    f.raw.eip = 0x4000f49d;
    f.raw.esp = stack.start().value().unwrap() as u32;
    // Make the frame pointer bogus, to make the stackwalker scan the stack
    // for something that looks like a return address.
    f.raw.ebp = 0xd43eed6e;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x4000f49d);

        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.eip, 0x4000f49d);
            assert_eq!(ctx.esp, stack_start as u32);
            assert_eq!(ctx.ebp, 0xd43eed6e);
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
        } else {
            unreachable!();
        }
        assert_eq!(f1.instruction + 1, 0x4000129d);

        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, 0x4000129d);
            assert_eq!(ctx.esp, frame1_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame1_ebp.value().unwrap() as u32);
        } else {
            unreachable!();
        }
    }
}

const CALLEE_SAVE_REGS: &[&str] = &["eip", "esp", "ebp", "ebx", "edi", "esi"];

fn init_cfi_state() -> (TestFixture, Section, CONTEXT_X86, MinidumpContextValidity) {
    let mut f = TestFixture::new();
    let symbols = [
        // The youngest frame's function.
        "FUNC 4000 1000 10 enchiridion\n",
        // Initially, just a return address.
        "STACK CFI INIT 4000 100 .cfa: $esp 4 + .ra: .cfa 4 - ^\n",
        // Push %ebx.
        "STACK CFI 4001 .cfa: $esp 8 + $ebx: .cfa 8 - ^\n",
        // Move %esi into %ebx.  Weird, but permitted.
        "STACK CFI 4002 $esi: $ebx\n",
        // Allocate frame space, and save %edi.
        "STACK CFI 4003 .cfa: $esp 20 + $edi: .cfa 16 - ^\n",
        // Put the return address in %edi.
        "STACK CFI 4005 .ra: $edi\n",
        // Save %ebp, and use it as a frame pointer.
        "STACK CFI 4006 .cfa: $ebp 8 + $ebp: .cfa 12 - ^\n",
        // The calling function.
        "FUNC 5000 1000 10 epictetus\n",
        // Mark it as end of stack.
        "STACK CFI INIT 5000 1000 .cfa: $esp .ra 0\n",
    ];
    f.add_symbols(String::from("module1"), symbols.concat());

    f.raw.set_register("esp", 0x80000000);
    f.raw.set_register("eip", 0x40005510);
    f.raw.set_register("ebp", 0xc0d4aab9);
    f.raw.set_register("ebx", 0x60f20ce6);
    f.raw.set_register("esi", 0x53d1379d);
    f.raw.set_register("edi", 0xafbae234);

    let raw_valid = MinidumpContextValidity::All;

    let expected = f.raw.clone();
    let expected_regs = CALLEE_SAVE_REGS;
    let expected_valid = MinidumpContextValidity::Some(expected_regs.iter().copied().collect());

    let stack = Section::new();
    stack
        .start()
        .set_const(f.raw.get_register("esp", &raw_valid).unwrap() as u64);

    (f, stack, expected, expected_valid)
}

async fn check_cfi(
    f: TestFixture,
    stack: Section,
    expected: CONTEXT_X86,
    expected_valid: MinidumpContextValidity,
) {
    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // Frame 0
        let frame = &s.frames[0];
        assert_eq!(frame.trust, FrameTrust::Context);
        assert_eq!(frame.context.valid, MinidumpContextValidity::All);
    }

    {
        // Frame 1
        if let MinidumpContextValidity::Some(ref expected_regs) = expected_valid {
            let frame = &s.frames[1];
            let valid = &frame.context.valid;
            assert_eq!(frame.trust, FrameTrust::CallFrameInfo);
            if let MinidumpContextValidity::Some(ref which) = valid {
                assert_eq!(which.len(), expected_regs.len());
            } else {
                unreachable!();
            }

            if let MinidumpRawContext::X86(ctx) = &frame.context.raw {
                for reg in expected_regs {
                    assert_eq!(
                        ctx.get_register(reg, valid),
                        expected.get_register(reg, &expected_valid),
                        "{reg} registers didn't match!"
                    );
                }
                return;
            }
        }
    }
    unreachable!();
}

#[tokio::test]
async fn test_cfi_at_4000() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0x40005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004000);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4001() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x40005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004001);
    f.raw.set_register("ebx", 0x91aa9a8b);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4002() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x40005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004002);
    f.raw.set_register("ebx", 0x53d1379d);
    f.raw.set_register("esi", 0xa5c790ed);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4003() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0x56ec3db7) // garbage
        .D32(0xafbae234) // saved %edi
        .D32(0x53d67131) // garbage
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x40005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004003);
    f.raw.set_register("ebx", 0x53d1379d);
    f.raw.set_register("esi", 0xa97f229d);
    f.raw.set_register("edi", 0xb05cc997);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4004() {
    // Should be the same as 4003
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0x56ec3db7) // garbage
        .D32(0xafbae234) // saved %edi
        .D32(0x53d67131) // garbage
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x40005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004004);
    f.raw.set_register("ebx", 0x53d1379d);
    f.raw.set_register("esi", 0xa97f229d);
    f.raw.set_register("edi", 0xb05cc997);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4005() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D32(0xe29782c2) // garbage
        .D32(0xafbae234) // saved %edi
        .D32(0x5ba29ce9) // garbage
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x8036cc02) // garbage
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004005);
    f.raw.set_register("ebx", 0x53d1379d);
    f.raw.set_register("esi", 0x0fb7dc4e);
    f.raw.set_register("edi", 0x40005510);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4006() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame0_ebp = Label::new();
    let frame1_rsp = Label::new();
    stack = stack
        .D32(0xdcdd25cd) // garbage
        .D32(0xafbae234) // saved %edi
        .D32(0xc0d4aab9) // saved %ebp
        .mark(&frame0_ebp) // frame pointer points here
        .D32(0x60f20ce6) // saved %ebx
        .D32(0x8036cc02) // garbage
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("esp", frame1_rsp.value().unwrap() as u32);
    f.raw
        .set_register("ebp", frame0_ebp.value().unwrap() as u32);
    f.raw.set_register("eip", 0x40004006);
    f.raw.set_register("ebx", 0x53d1379d);
    f.raw.set_register("esi", 0x743833c9);
    f.raw.set_register("edi", 0x40005510);

    check_cfi(f, stack, expected, expected_valid).await;
}

// Totally basic STACK WIN frame data, no weird stuff.
#[tokio::test]
async fn test_stack_win_frame_data_basic() {
    let mut f = TestFixture::new();
    let symbols = [
        "STACK WIN 4 aa85 176 0 0 4 10 4 0 1",
        " $T2 $esp .cbSavedRegs + =",
        " $T0 .raSearchStart =",
        " $eip $T0 ^ =",
        " $esp $T0 4 + =",
        " $ebx $T2 4  - ^ =",
        " $edi $T2 8  - ^ =",
        " $esi $T2 12 - ^ =",
        " $ebp $T2 16 - ^ =\n",
    ];
    f.add_symbols(String::from("module1"), symbols.concat());

    let frame1_esp = Label::new();
    let frame1_ebp = Label::new();

    let mut stack = Section::new();
    let stack_start = 0x80000000;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0
        .D32(&frame1_ebp) // saved regs: %ebp
        .D32(0xa7120d1a) //             %esi
        .D32(0x630891be) //             %edi
        .D32(0x9068a878) //             %ebx
        .D32(0xa08ea45f) // locals: unused
        .D32(0x40001350) // return address
        // frame 1
        .mark(&frame1_esp)
        .append_repeated(0, 12) // empty space
        .mark(&frame1_ebp)
        .D32(0) // saved %ebp (stack end)
        .D32(0); // saved %eip (stack end)

    f.raw.set_register("eip", 0x4000aa85);
    f.raw
        .set_register("esp", stack.start().value().unwrap() as u32);
    f.raw.set_register("ebp", 0xf052c1de);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x4000aa85);

        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.eip, 0x4000aa85);
            assert_eq!(ctx.esp, stack_start as u32);
            assert_eq!(ctx.ebp, 0xf052c1de);
        } else {
            unreachable!();
        }
    }

    {
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::CallFrameInfo);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
            assert!(which.contains("ebx"));
            assert!(which.contains("esi"));
            assert!(which.contains("edi"));
        } else {
            unreachable!();
        }
        assert_eq!(f1.instruction + 1, 0x40001350);

        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, 0x40001350);
            assert_eq!(ctx.esp, frame1_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame1_ebp.value().unwrap() as u32);
            assert_eq!(ctx.ebx, 0x9068a878);
            assert_eq!(ctx.esi, 0xa7120d1a);
            assert_eq!(ctx.edi, 0x630891be);
        } else {
            unreachable!();
        }
    }
}

// Totally basic STACK WIN frame data, no weird stuff.
#[tokio::test]
async fn test_stack_win_frame_data_overlapping() {
    // Same as frame_data_basic but there are extra entries which technically overlap
    // with this one, but in a way that is easily disambiguated by preferring the
    // one with the higher base address. This happens frequently in real symbol files.
    let mut f = TestFixture::new();
    let symbols = [
        // Entry that covers the "whole" function (junk!)
        "STACK WIN 4 aa80 181 0 0 4 10 4 0 1",
        " $eip .raSearchStart =\n",
        // More precise (still junk!)
        "STACK WIN 4 aa84 177 0 0 4 10 4 0 1",
        " $eip .raSearchStart =\n",
        // This is the one we want!!!
        "STACK WIN 4 aa85 176 0 0 4 10 4 0 1",
        " $T2 $esp .cbSavedRegs + =",
        " $T0 .raSearchStart =",
        " $eip $T0 ^ =",
        " $esp $T0 4 + =",
        " $ebx $T2 4  - ^ =",
        " $edi $T2 8  - ^ =",
        " $esi $T2 12 - ^ =",
        " $ebp $T2 16 - ^ =\n",
        // An even more precise one but past the address we care about (junk!)
        "STACK WIN 4 aa86 175 0 0 4 10 4 0 1",
        " $eip .raSearchStart =\n",
    ];
    f.add_symbols(String::from("module1"), symbols.concat());

    let frame1_esp = Label::new();
    let frame1_ebp = Label::new();

    let mut stack = Section::new();
    let stack_start = 0x80000000;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0
        .D32(&frame1_ebp) // saved regs: %ebp
        .D32(0xa7120d1a) //             %esi
        .D32(0x630891be) //             %edi
        .D32(0x9068a878) //             %ebx
        .D32(0xa08ea45f) // locals: unused
        .D32(0x40001350) // return address
        // frame 1
        .mark(&frame1_esp)
        .append_repeated(0, 12) // empty space
        .mark(&frame1_ebp)
        .D32(0) // saved %ebp (stack end)
        .D32(0); // saved %eip (stack end)

    f.raw.set_register("eip", 0x4000aa85);
    f.raw
        .set_register("esp", stack.start().value().unwrap() as u32);
    f.raw.set_register("ebp", 0xf052c1de);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x4000aa85);

        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.eip, 0x4000aa85);
            assert_eq!(ctx.esp, stack_start as u32);
            assert_eq!(ctx.ebp, 0xf052c1de);
        } else {
            unreachable!();
        }
    }

    {
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::CallFrameInfo);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
            assert!(which.contains("ebx"));
            assert!(which.contains("esi"));
            assert!(which.contains("edi"));
        } else {
            unreachable!();
        }
        assert_eq!(f1.instruction + 1, 0x40001350);

        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, 0x40001350);
            assert_eq!(ctx.esp, frame1_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame1_ebp.value().unwrap() as u32);
            assert_eq!(ctx.ebx, 0x9068a878);
            assert_eq!(ctx.esi, 0xa7120d1a);
            assert_eq!(ctx.edi, 0x630891be);
        } else {
            unreachable!();
        }
    }
}

// Testing that grand_callee_parameter_size is properly computed.
#[tokio::test]
async fn test_stack_win_frame_data_parameter_size() {
    let mut f = TestFixture::new();

    let module1_symbols = ["FUNC 1000 100 c module1::wheedle\n"];

    let module2_symbols = [
        // Note bogus parameter size in FUNC record; the stack walker
        // should prefer the STACK WIN record, and see '4' below.
        "FUNC aa85 176 beef module2::whine\n",
        "STACK WIN 4 aa85 176 0 0 4 10 4 0 1",
        " $T2 $esp .cbLocals + .cbSavedRegs + =",
        " $T0 .raSearchStart =",
        " $eip $T0 ^ =",
        " $esp $T0 4 + =",
        " $ebp $T0 20 - ^ =",
        " $ebx $T0 8 - ^ =\n",
    ];
    f.add_symbols(String::from("module1"), module1_symbols.concat());
    f.add_symbols(String::from("module2"), module2_symbols.concat());

    let frame0_esp = Label::new();
    let frame0_ebp = Label::new();
    let frame1_esp = Label::new();
    let frame2_esp = Label::new();
    let frame2_ebp = Label::new();

    let mut stack = Section::new();
    let stack_start = 0x80000000;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0, in module1::wheedle.  Traditional frame.
        .mark(&frame0_esp)
        .append_repeated(0, 16) // frame space
        .mark(&frame0_ebp)
        .D32(0x6fa902e0) // saved %ebp.  Not a frame pointer.
        .D32(0x5000aa95) // return address, in module2::whine
        // frame 1, in module2::whine.  FrameData frame.
        .mark(&frame1_esp)
        .D32(0xbaa0cb7a) // argument 3 passed to module1::wheedle
        .D32(0xbdc92f9f) // argument 2
        .D32(0x0b1d8442) // argument 1
        .D32(&frame2_ebp) // saved %ebp
        .D32(0xb1b90a15) // unused
        .D32(0xf18e072d) // unused
        .D32(0x2558c7f3) // saved %ebx
        .D32(0x0365e25e) // unused
        .D32(0x2a179e38) // return address; $T0 points here
        // frame 2, in no module
        .mark(&frame2_esp)
        .append_repeated(0, 12) // empty space
        .mark(&frame2_ebp)
        .D32(0) // saved %ebp (stack end)
        .D32(0); // saved %eip (stack end)

    f.raw.set_register("eip", 0x40001004);
    f.raw
        .set_register("esp", stack.start().value().unwrap() as u32);
    f.raw
        .set_register("ebp", frame0_ebp.value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 3);

    {
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        assert_eq!(f0.instruction, 0x40001004);

        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.eip, 0x40001004);
            assert_eq!(ctx.esp, frame0_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame0_ebp.value().unwrap() as u32);
        } else {
            unreachable!();
        }
    }

    {
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
        } else {
            unreachable!();
        }
        assert_eq!(f1.instruction + 1, 0x5000aa95);

        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, 0x5000aa95);
            assert_eq!(ctx.esp, frame1_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, 0x6fa902e0);
        } else {
            unreachable!();
        }
    }

    {
        let f2 = &s.frames[2];
        assert_eq!(f2.trust, FrameTrust::CallFrameInfo);
        if let MinidumpContextValidity::Some(ref which) = f2.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
            assert!(which.contains("ebx"));
        } else {
            unreachable!();
        }
        assert_eq!(f2.instruction + 1, 0x2a179e38);

        if let MinidumpRawContext::X86(ctx) = &f2.context.raw {
            assert_eq!(ctx.eip, 0x2a179e38);
            assert_eq!(ctx.esp, frame2_esp.value().unwrap() as u32);
            assert_eq!(ctx.ebp, frame2_ebp.value().unwrap() as u32);
            assert_eq!(ctx.ebx, 0x2558c7f3);
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_frame_pointer_overflow() {
    // Make sure we don't explode when trying frame pointer analysis on a value
    // that will overflow.

    type Pointer = u32;
    let stack_max: Pointer = Pointer::MAX;
    let stack_size: Pointer = 1000;
    let bad_frame_ptr: Pointer = stack_max;

    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start: Pointer = stack_max - stack_size;
    stack.start().set_const(stack_start as u64);

    stack = stack
        // frame 0
        .append_repeated(0, stack_size as usize); // junk, not important to the test

    f.raw.eip = 0x7a100000;
    f.raw.ebp = bad_frame_ptr;
    f.raw.esp = stack.start().value().unwrap() as Pointer;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);

    // As long as we don't panic, we're good!
}

#[tokio::test]
async fn test_frame_pointer_overflow_nonsense_32bit_stack() {
    // same as test_frame_pointer_overflow, but we're going to abuse the fact
    // that rust-minidump prefers representing things in 64-bit to create
    // impossible stack addresses that overflow 32-bit integers but appear
    // valid in 64-bit. By doing this memory reads will "succeed" but
    // pointer math done in the native pointer width will overflow and
    // everything will be sad.

    type Pointer = u32;
    let pointer_size: u64 = std::mem::size_of::<Pointer>() as u64;
    let stack_max: u64 = Pointer::MAX as u64 + pointer_size * 2;
    let stack_size: u64 = 1000;
    let bad_frame_ptr: u64 = Pointer::MAX as u64 - pointer_size;

    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start: u64 = stack_max - stack_size;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0
        .append_repeated(0, 1000); // junk, not important to the test

    f.raw.eip = 0x7a100000;
    f.raw.ebp = bad_frame_ptr as u32;
    f.raw.esp = stack.start().value().unwrap() as Pointer;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);

    // As long as we don't panic, we're good!
}

#[tokio::test]
async fn test_frame_pointer_barely_no_overflow() {
    // This is test_tradition but with the all the values pushed
    // as close to the upper memory boundary as possible, to confirm that
    // our code doesn't randomly overflow *AND* isn't overzealous in
    // its overflow guards.

    let mut f = TestFixture::new();
    let mut stack = Section::new();

    type Pointer = u32;
    let pointer_size: Pointer = std::mem::size_of::<Pointer>() as Pointer;
    let stack_max: Pointer = Pointer::MAX;
    let stack_size: Pointer = pointer_size * 3;

    let stack_start: Pointer = stack_max - stack_size;
    let return_address: Pointer = 0x7b302000;
    stack.start().set_const(stack_start as u64);

    let frame0_fp = Label::new();
    let frame1_sp = Label::new();
    let frame1_fp = Label::new();

    stack = stack
        // frame 0
        .mark(&frame0_fp)
        .D32(&frame1_fp) // caller-pushed %rbp
        .D32(return_address) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .mark(&frame1_fp) // end of stack
        .D32(0);

    f.raw.eip = 0x7a100000;
    f.raw.ebp = frame0_fp.value().unwrap() as Pointer;
    f.raw.esp = stack.start().value().unwrap() as Pointer;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        if let MinidumpRawContext::X86(ctx) = &f0.context.raw {
            assert_eq!(ctx.ebp, frame0_fp.value().unwrap() as Pointer);
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("eip"));
            assert!(which.contains("esp"));
            assert!(which.contains("ebp"));
        } else {
            unreachable!();
        }
        if let MinidumpRawContext::X86(ctx) = &f1.context.raw {
            assert_eq!(ctx.eip, return_address);
            assert_eq!(ctx.esp, frame1_sp.value().unwrap() as Pointer);
            assert_eq!(ctx.ebp, frame1_fp.value().unwrap() as Pointer);
        } else {
            unreachable!();
        }
    }
}
