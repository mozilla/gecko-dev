// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use crate::*;
use minidump::format::CONTEXT_AMD64;
use minidump::system_info::{Cpu, Os};
use std::collections::HashMap;
use test_assembler::*;

struct TestFixture {
    pub raw: CONTEXT_AMD64,
    pub modules: MinidumpModuleList,
    pub system_info: SystemInfo,
    pub symbols: HashMap<String, String>,
}

impl TestFixture {
    pub fn new() -> TestFixture {
        TestFixture {
            raw: CONTEXT_AMD64::default(),
            // Give the two modules reasonable standard locations and names
            // for tests to play with.
            modules: MinidumpModuleList::from_modules(vec![
                MinidumpModule::new(0x00007400c0000000, 0x10000, "module1"),
                MinidumpModule::new(0x00007500b0000000, 0x10000, "module2"),
            ]),
            system_info: SystemInfo {
                os: Os::Linux,
                os_version: None,
                os_build: None,
                cpu: Cpu::X86_64,
                cpu_info: None,
                cpu_microcode_version: None,
                cpu_count: 1,
            },
            symbols: HashMap::new(),
        }
    }

    pub async fn walk_stack(&self, stack: Section) -> CallStack {
        let context = MinidumpContext {
            raw: MinidumpRawContext::Amd64(self.raw.clone()),
            valid: MinidumpContextValidity::All,
        };
        let base = stack.start().value().unwrap();
        let size = stack.size();
        let stack = stack.get_contents().unwrap();
        let stack_memory = &MinidumpMemory {
            desc: Default::default(),
            base_address: base,
            size,
            bytes: &stack,
            endian: scroll::LE,
        };
        let symbolizer = Symbolizer::new(string_symbol_supplier(self.symbols.clone()));
        let mut stack = CallStack::with_context(context);

        walk_stack(
            0,
            (),
            &mut stack,
            Some(UnifiedMemory::Memory(stack_memory)),
            &self.modules,
            &self.system_info,
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
    let stack = Section::new();
    stack.start().set_const(0x80000000);
    // There should be no references to the stack in this walk: we don't
    // provide any call frame information, so trying to reconstruct the
    // context frame's caller should fail. So there's no need for us to
    // provide stack contents.
    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = 0x8000000080000000;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);
    let f = &s.frames[0];
    let m = f.module.as_ref().unwrap();
    assert_eq!(m.code_file(), "module1");
}

#[tokio::test]
async fn test_caller_pushed_rbp() {
    // Functions typically push their %rbp upon entry and set %rbp pointing
    // there.  If stackwalking finds a plausible address for the next frame's
    // %rbp directly below the return address, assume that it is indeed the
    // next frame's %rbp.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start = 0x8000000080000000;
    let return_address = 0x00007500b0000110;
    stack.start().set_const(stack_start);

    let frame0_rbp = Label::new();
    let frame1_sp = Label::new();
    let frame1_rbp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D64(0x00007400b0000000) // junk that's not
        .D64(0x00007500b0000000) // a return address
        .D64(0x00007400c0001000) // a couple of plausible addresses
        .D64(0x00007500b000aaaa) // that are not within functions
        .mark(&frame0_rbp)
        .D64(&frame1_rbp) // caller-pushed %rbp
        .D64(return_address) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 32) // body of frame1
        .mark(&frame1_rbp) // end of stack
        .D64(0);

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame0_rbp.value().unwrap();
    f.raw.rsp = stack.start().value().unwrap();

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        if let MinidumpRawContext::Amd64(ctx) = &f0.context.raw {
            assert_eq!(ctx.rbp, frame0_rbp.value().unwrap());
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
            assert!(which.contains("rbp"));
        } else {
            unreachable!();
        }
        if let MinidumpRawContext::Amd64(ctx) = &f1.context.raw {
            assert_eq!(ctx.rip, return_address);
            assert_eq!(ctx.rsp, frame1_sp.value().unwrap());
            assert_eq!(ctx.rbp, frame1_rbp.value().unwrap());
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_windows_rbp_scan() {
    let mut f = TestFixture::new();
    f.system_info.os = Os::Windows;

    let mut stack = Section::new();
    let stack_start = 0x8000000080000000;
    let return_address = 0x00007500b0000110;
    stack.start().set_const(stack_start);

    let frame0_rbp = Label::new();
    let frame1_sp = Label::new();
    let frame1_rbp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D64(0x00000000b0000000) // junk that's not
        .D64(0x00000000b0000000) // a return address
        .mark(&frame0_rbp) // the FP can point to the middle of the stack on Windows
        .D64(0x00000000c0001000)
        .D64(0x00000000b000aaaa)
        .D64(&frame1_rbp) // caller-pushed %rbp
        .D64(return_address) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 32) // body of frame1
        .mark(&frame1_rbp) // end of stack
        .D64(0);

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame0_rbp.value().unwrap();
    f.raw.rsp = stack.start().value().unwrap();

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        if let MinidumpRawContext::Amd64(ctx) = &f0.context.raw {
            assert_eq!(ctx.rbp, frame0_rbp.value().unwrap());
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
        } else {
            unreachable!();
        }
        if let MinidumpRawContext::Amd64(ctx) = &f1.context.raw {
            assert_eq!(ctx.rip, return_address);
            assert_eq!(ctx.rsp, frame1_sp.value().unwrap());
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_scan_without_symbols() {
    // When the stack walker resorts to scanning the stack,
    // only addresses located within loaded modules are
    // considered valid return addresses.
    // Force scanning through three frames to ensure that the
    // stack pointer is set properly in scan-recovered frames.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start = 0x8000000080000000;
    stack.start().set_const(stack_start);

    let return_address1 = 0x00007500b0000100;
    let return_address2 = 0x00007500b0000900;

    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame1_rbp = Label::new();
    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D64(0x00007400b0000000) // junk that's not
        .D64(0x00007500d0000000) // a return address
        .D64(return_address1) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 16) // space
        .D64(0x00007400b0000000) // more junk
        .D64(0x00007500d0000000)
        .mark(&frame1_rbp)
        .D64(stack_start) // This is in the right place to be
        // a saved rbp, but it's bogus, so
        // we shouldn't report it.
        .D64(return_address2) // actual return address
        // frame 2
        .mark(&frame2_sp)
        .append_repeated(0, 32); // end of stack

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame1_rbp.value().unwrap();
    f.raw.rsp = stack.start().value().unwrap();

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 3);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
            assert!(which.contains("rbp"));
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Amd64(ctx) = &f1.context.raw {
            assert_eq!(ctx.rip, return_address1);
            assert_eq!(ctx.rsp, frame1_sp.value().unwrap());
            assert_eq!(ctx.rbp, frame1_rbp.value().unwrap());
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f2 = &s.frames[2];
        assert_eq!(f2.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f2.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Amd64(ctx) = &f2.context.raw {
            assert_eq!(ctx.rip, return_address2);
            assert_eq!(ctx.rsp, frame2_sp.value().unwrap());
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_scan_with_symbols() {
    // Test that we can refine our scanning using symbols. Specifically we
    // should be able to reject pointers that are in modules but don't map to
    // any FUNC/PUBLIC record.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start = 0x8000000080000000u64;
    stack.start().set_const(stack_start);

    let return_address = 0x00007500b0000110u64;

    let frame1_rsp = Label::new();
    let frame1_rbp = Label::new();
    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D64(0x00007400b0000000u64) // junk that's not
        .D64(0x00007500b0000000u64) // a return address
        .D64(0x00007400c0001000u64) // a couple of plausible addresses
        .D64(0x00007500b000aaaau64) // that are not within functions
        .D64(return_address) // actual return address
        // frame 1
        .mark(&frame1_rsp)
        .append_repeated(0, 32)
        .mark(&frame1_rbp); // end of stack

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame1_rbp.value().unwrap();
    f.raw.rsp = stack.start().value().unwrap();

    f.add_symbols(
        String::from("module1"),
        // The youngest frame's function.
        String::from("FUNC 100 400 10 monotreme\n"),
    );
    f.add_symbols(
        String::from("module2"),
        // The calling frame's function.
        String::from("FUNC 100 400 10 marsupial\n"),
    );

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
        let frame = &s.frames[1];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Amd64(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("rip", valid).unwrap(), return_address);
            assert_eq!(
                ctx.get_register("rsp", valid).unwrap(),
                frame1_rsp.value().unwrap()
            );
            assert_eq!(
                ctx.get_register("rbp", valid).unwrap(),
                frame1_rbp.value().unwrap()
            );
        } else {
            unreachable!();
        }
    }
}

const CALLEE_SAVE_REGS: &[&str] = &["rip", "rbx", "rbp", "rsp", "r12", "r13", "r14", "r15"];

fn init_cfi_state() -> (TestFixture, Section, CONTEXT_AMD64, MinidumpContextValidity) {
    let mut f = TestFixture::new();
    let symbols = [
        // The youngest frame's function.
        "FUNC 4000 1000 10 enchiridion\n",
        // Initially, just a return address.
        "STACK CFI INIT 4000 100 .cfa: $rsp 8 + .ra: .cfa 8 - ^\n",
        // Push %rbx.
        "STACK CFI 4001 .cfa: $rsp 16 + $rbx: .cfa 16 - ^\n",
        // Save %r12 in %rbx.  Weird, but permitted.
        "STACK CFI 4002 $r12: $rbx\n",
        // Allocate frame space, and save %r13.
        "STACK CFI 4003 .cfa: $rsp 40 + $r13: .cfa 32 - ^\n",
        // Put the return address in %r13.
        "STACK CFI 4005 .ra: $r13\n",
        // Save %rbp, and use it as a frame pointer.
        "STACK CFI 4006 .cfa: $rbp 16 + $rbp: .cfa 24 - ^\n",
        // The calling function.
        "FUNC 5000 1000 10 epictetus\n",
        // Mark it as end of stack.
        "STACK CFI INIT 5000 1000 .cfa: $rsp .ra 0\n",
    ];
    f.add_symbols(String::from("module1"), symbols.concat());

    f.raw.set_register("rsp", 0x8000000080000000);
    f.raw.set_register("rip", 0x00007400c0005510);
    f.raw.set_register("rbp", 0x68995b1de4700266);
    f.raw.set_register("rbx", 0x5a5beeb38de23be8);
    f.raw.set_register("r12", 0xed1b02e8cc0fc79c);
    f.raw.set_register("r13", 0x1d20ad8acacbe930);
    f.raw.set_register("r14", 0xe94cffc2f7adaa28);
    f.raw.set_register("r15", 0xb638d17d8da413b5);

    let raw_valid = MinidumpContextValidity::All;

    let expected = f.raw.clone();
    let expected_regs = CALLEE_SAVE_REGS;
    let expected_valid = MinidumpContextValidity::Some(expected_regs.iter().copied().collect());

    let stack = Section::new();
    stack
        .start()
        .set_const(f.raw.get_register("rsp", &raw_valid).unwrap());

    (f, stack, expected, expected_valid)
}

async fn check_cfi(
    f: TestFixture,
    stack: Section,
    expected: CONTEXT_AMD64,
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

            if let MinidumpRawContext::Amd64(ctx) = &frame.context.raw {
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
        .D64(0x00007400c0005510)
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004000);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4001() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0x00007400c0005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004001);
    f.raw.set_register("rbx", 0xbe0487d2f9eafe29);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4002() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0x00007400c0005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004002);
    f.raw.set_register("rbx", 0xed1b02e8cc0fc79c); // saved %r12
    f.raw.set_register("r12", 0xb0118de918a4bcea); // callee's (distinct) %r12 value

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4003() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x0e023828dffd4d81) // garbage
        .D64(0x1d20ad8acacbe930) // saved %r13
        .D64(0x319e68b49e3ace0f) // garbage
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0x00007400c0005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004003);
    f.raw.set_register("rbx", 0xed1b02e8cc0fc79c); // saved %r12
    f.raw.set_register("r12", 0x89d04fa804c87a43); // callee's (distinct) %r12
    f.raw.set_register("r13", 0x5118e02cbdb24b03); // callee's (distinct) %r13

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4004() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x0e023828dffd4d81) // garbage
        .D64(0x1d20ad8acacbe930) // saved %r13
        .D64(0x319e68b49e3ace0f) // garbage
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0x00007400c0005510) // return address
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004004);
    f.raw.set_register("rbx", 0xed1b02e8cc0fc79c); // saved %r12
    f.raw.set_register("r12", 0x46b1b8868891b34a); // callee's (distinct) %r12
    f.raw.set_register("r13", 0x5118e02cbdb24b03); // callee's (distinct) %r13

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4005() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x4b516dd035745953) // garbage
        .D64(0x1d20ad8acacbe930) // saved %r13
        .D64(0xa6d445e16ae3d872) // garbage
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0xaa95fa054aedfbae) // garbage
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004005);
    f.raw.set_register("rbx", 0xed1b02e8cc0fc79c); // saved %r12
    f.raw.set_register("r12", 0x46b1b8868891b34a); // callee's %r12
    f.raw.set_register("r13", 0x00007400c0005510); // return address

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4006() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame0_rbp = Label::new();
    let frame1_rsp = Label::new();
    stack = stack
        .D64(0x043c6dfceb91aa34) // garbage
        .D64(0x1d20ad8acacbe930) // saved %r13
        .D64(0x68995b1de4700266) // saved %rbp
        .mark(&frame0_rbp) // frame pointer points here
        .D64(0x5a5beeb38de23be8) // saved %rbx
        .D64(0xf015ee516ad89eab) // garbage
        .mark(&frame1_rsp)
        .append_repeated(0, 1000);

    expected.set_register("rsp", frame1_rsp.value().unwrap());
    f.raw.set_register("rip", 0x00007400c0004006);
    f.raw.set_register("rbp", frame0_rbp.value().unwrap());
    f.raw.set_register("rbx", 0xed1b02e8cc0fc79c); // saved %r12
    f.raw.set_register("r12", 0x26e007b341acfebd); // callee's %r12
    f.raw.set_register("r13", 0x00007400c0005510); // return address

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_frame_pointer_overflow() {
    // Make sure we don't explode when trying frame pointer analysis on a value
    // that will overflow.

    type Pointer = u64;
    let stack_max: Pointer = Pointer::MAX;
    let stack_size: Pointer = 1000;
    let bad_frame_ptr: Pointer = stack_max;

    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start: Pointer = stack_max - stack_size;
    stack.start().set_const(stack_start);

    stack = stack
        // frame 0
        .append_repeated(0, stack_size as usize); // junk, not important to the test

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = bad_frame_ptr;
    f.raw.rsp = stack.start().value().unwrap() as Pointer;

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);

    // As long as we don't panic, we're good!
}

#[tokio::test]
async fn test_frame_pointer_barely_no_overflow() {
    // This is test_caller_pushed_rbp but with the all the values pushed
    // as close to the upper memory boundary as possible, to confirm that
    // our code doesn't randomly overflow *AND* isn't overzealous in
    // its overflow guards.

    let mut f = TestFixture::new();
    let mut stack = Section::new();

    type Pointer = u64;
    let stack_max: Pointer = Pointer::MAX;
    let pointer_size: Pointer = std::mem::size_of::<Pointer>() as Pointer;
    let stack_size: Pointer = pointer_size * 3;

    let stack_start: Pointer = stack_max - stack_size;
    let return_address: Pointer = 0x00007500b0000110;
    stack.start().set_const(stack_start);

    let frame0_fp = Label::new();
    let frame1_sp = Label::new();
    let frame1_fp = Label::new();

    stack = stack
        // frame 0
        .mark(&frame0_fp)
        .D64(&frame1_fp) // caller-pushed %rbp
        .D64(return_address) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .mark(&frame1_fp) // end of stack
        .D64(0);

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame0_fp.value().unwrap() as Pointer;
    f.raw.rsp = stack.start().value().unwrap();

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
        if let MinidumpRawContext::Amd64(ctx) = &f0.context.raw {
            assert_eq!(ctx.rbp, frame0_fp.value().unwrap() as Pointer);
        } else {
            unreachable!();
        }
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
            assert!(which.contains("rbp"));
        } else {
            unreachable!();
        }
        if let MinidumpRawContext::Amd64(ctx) = &f1.context.raw {
            assert_eq!(ctx.rip, return_address);
            assert_eq!(ctx.rsp, frame1_sp.value().unwrap() as Pointer);
            assert_eq!(ctx.rbp, frame1_fp.value().unwrap() as Pointer);
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_scan_walk_overflow() {
    // There's a possible overflow when address_of_ip starts out at 0.
    //
    // To avoid this, we only try to recover rbp when we're scanning at least
    // 1 pointer width away from the start of the stack.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    let stack_start = 0;
    stack.start().set_const(stack_start);

    let return_address1 = 0x00007500b0000100_u64;

    let frame1_sp = Label::new();
    let frame1_rbp = Label::new();

    stack = stack
        // frame 0
        .D64(return_address1) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 16) // space
        .D64(0x00007400b0000000) // more junk
        .D64(0x00007500d0000000)
        .mark(&frame1_rbp);

    f.raw.rip = 0x00007400c0000200;
    f.raw.rbp = frame1_rbp.value().unwrap();
    f.raw.rsp = stack.start().value().unwrap();

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // To avoid reusing locals by mistake
        let f0 = &s.frames[0];
        assert_eq!(f0.trust, FrameTrust::Context);
        assert_eq!(f0.context.valid, MinidumpContextValidity::All);
    }

    {
        // To avoid reusing locals by mistake
        let f1 = &s.frames[1];
        assert_eq!(f1.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = f1.context.valid {
            assert!(which.contains("rip"));
            assert!(which.contains("rsp"));
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Amd64(ctx) = &f1.context.raw {
            assert_eq!(ctx.rip, return_address1);
            assert_eq!(ctx.rsp, frame1_sp.value().unwrap());
            // We were unable to recover rbp, so it defaulted to 0.
            assert_eq!(ctx.rbp, 0);
        } else {
            unreachable!();
        }
    }
}
