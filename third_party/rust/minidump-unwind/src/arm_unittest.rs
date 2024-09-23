// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use crate::*;
use minidump::format::CONTEXT_ARM;
use minidump::system_info::{Cpu, Os};
use std::collections::HashMap;
use test_assembler::*;

struct TestFixture {
    pub raw: CONTEXT_ARM,
    pub modules: MinidumpModuleList,
    pub system_info: SystemInfo,
    pub symbols: HashMap<String, String>,
}

impl TestFixture {
    pub fn new() -> TestFixture {
        TestFixture {
            raw: CONTEXT_ARM::default(),
            // Give the two modules reasonable standard locations and names
            // for tests to play with.
            modules: MinidumpModuleList::from_modules(vec![
                MinidumpModule::new(0x40000000, 0x10000, "module1"),
                MinidumpModule::new(0x50000000, 0x10000, "module2"),
            ]),
            system_info: SystemInfo {
                os: Os::Ios,
                os_version: None,
                os_build: None,
                cpu: Cpu::Arm,
                cpu_info: None,
                cpu_microcode_version: None,
                cpu_count: 1,
            },
            symbols: HashMap::new(),
        }
    }

    pub async fn walk_stack(&self, stack: Section) -> CallStack {
        let context = MinidumpContext {
            raw: MinidumpRawContext::Arm(self.raw.clone()),
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
        let symbolizer = Symbolizer::new(string_symbol_supplier(self.symbols.clone()));
        let mut stack = CallStack::with_context(context);

        walk_stack(
            0,
            (),
            &mut stack,
            Some(UnifiedMemory::Memory(&stack_memory)),
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
    f.raw.set_register("pc", 0x4000c020);
    f.raw.set_register("fp", 0x80000000);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);
    let f = &s.frames[0];
    let m = f.module.as_ref().unwrap();
    assert_eq!(m.code_file(), "module1");
}

#[tokio::test]
async fn test_scan_without_symbols() {
    // Scanning should work without any symbols
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D32(0x40090000) // junk that's not
        .D32(0x60000000) // a return address
        .D32(return_address1) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 16) // space
        .D32(0xF0000000) // more junk
        .D32(0x0000000D)
        .D32(return_address2) // actual return address
        // frame 2
        .mark(&frame2_sp)
        .append_repeated(0, 32); // end of stack

    f.raw.set_register("pc", 0x40005510);
    // set an invalid non-zero value for the frame pointer
    // to force stack scanning
    f.raw.set_register("fp", 0x00000001);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 3);

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
            assert_eq!(which.len(), 2);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address1);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }

    {
        // Frame 2
        let frame = &s.frames[2];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 2);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address2);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame2_sp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_scan_first_frame() {
    // The first (context) frame gets extra long scans, this test checks that.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 16) // space
        .D32(0x40090000) // junk that's not
        .D32(0x60000000) // a return address
        .append_repeated(0, 96) // more space
        .D32(return_address1) // actual return address
        // frame 1
        .mark(&frame1_sp)
        .append_repeated(0, 32) // space
        .D32(0xF0000000) // more junk
        .D32(0x0000000D)
        .append_repeated(0, 336) // more space
        .D32(return_address2) // actual return address (won't be found)
        // frame 2
        .mark(&frame2_sp)
        .append_repeated(0, 64); // end of stack

    f.raw.set_register("pc", 0x40005510);
    // set an invalid non-zero value for the frame pointer
    // to force stack scanning
    f.raw.set_register("fp", 0x00000001);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

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
            assert_eq!(which.len(), 2);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address1);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_invalid_lr() {
    let mut f = TestFixture::new();
    f.system_info.os = Os::Linux;

    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let lr = Label::new();
    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame1_fp = Label::new();
    let frame2_fp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 32) // space
        .mark(&lr) // the LR points to something on the stack
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame1_fp) // next fp will point to the next value
        .D32(&frame2_fp) // save current frame pointer
        .D32(return_address1) // save current link register
        .mark(&frame1_sp)
        // frame 1
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame2_fp)
        .D32(0)
        .D32(return_address2)
        .mark(&frame2_sp);

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("lr", lr.value().unwrap() as u32);
    f.raw.set_register("fp", frame1_fp.value().unwrap() as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 3);

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
            assert_eq!(which.len(), 2);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address1);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }

    {
        // Frame 2
        let frame = &s.frames[2];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::Scan);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 2);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address2);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame2_sp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_frame_pointer() {
    // Frame-pointer-based unwinding
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame0_fp = Label::new();
    let frame1_fp = Label::new();
    let frame2_fp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame0_fp) // next fp will point to the next value
        .D32(&frame1_fp) // save current frame pointer
        .D32(return_address1) // save current link register
        .mark(&frame1_sp)
        // frame 1
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame1_fp)
        .D32(&frame2_fp)
        .D32(return_address2)
        .mark(&frame2_sp)
        // frame 2
        .append_repeated(0, 32) // Whatever values on the stack.
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address.
        .mark(&frame2_fp)
        .D32(0)
        .D32(0);

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("lr", return_address1);
    f.raw.set_register("fp", frame0_fp.value().unwrap() as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 3);

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
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address1);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as u32
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame1_fp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }

    {
        // Frame 2
        let frame = &s.frames[2];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address2);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame2_sp.value().unwrap() as u32
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame2_fp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_frame_pointer_stackless_leaf() {
    // Same as test_frame_pointer but frame0 is a stackless leaf.
    //
    // In the current implementation we will misunderstand this slightly
    // and basically "lose" frame 1, but still properly recover frame 2.
    // THIS TEST BREAKING MIGHT MEAN YOU'VE MADE THINGS WORK BETTER!
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame1_fp = Label::new();
    let frame2_fp = Label::new();

    stack = stack
        // frame 0 (literally nothing!)
        .mark(&frame1_sp)
        // frame 1 (this is sadly dropped)
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame1_fp)
        .D32(&frame2_fp)
        .D32(return_address2)
        .mark(&frame2_sp)
        // frame 2
        .append_repeated(0, 32) // Whatever values on the stack.
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address.
        .mark(&frame2_fp)
        .D32(0)
        .D32(0);

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("lr", return_address1); // we will sadly ignore this
    f.raw.set_register("fp", frame1_fp.value().unwrap() as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // Frame 0
        let frame = &s.frames[0];
        assert_eq!(frame.trust, FrameTrust::Context);
        assert_eq!(frame.context.valid, MinidumpContextValidity::All);
    }

    {
        // Frame 2 (Found as Frame 1)
        let frame = &s.frames[1];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address2);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame2_sp.value().unwrap() as u32
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame2_fp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_frame_pointer_stackful_leaf() {
    // Same as test_frame_pointer but frame0 is a stackful leaf.
    //
    // In the current implementation we will misunderstand this slightly
    // and basically "lose" frame 1, but still properly recover frame 2.
    // THIS TEST BREAKING MIGHT MEAN YOU'VE MADE THINGS WORK BETTER!
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame1_fp = Label::new();
    let frame2_fp = Label::new();

    stack = stack
        // frame 0 (all junk!)
        .append_repeated(0, 64) // space
        .D64(0x0000000D) // junk that's not
        .D64(0xF0000000) // a return address
        .mark(&frame1_sp)
        // frame 1 (this is sadly dropped)
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame1_fp)
        .D32(&frame2_fp)
        .D32(return_address2)
        .mark(&frame2_sp)
        // frame 2
        .append_repeated(0, 32) // Whatever values on the stack.
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address.
        .mark(&frame2_fp)
        .D32(0)
        .D32(0);

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("lr", return_address1); // we will sadly ignore this
    f.raw.set_register("fp", frame1_fp.value().unwrap() as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // Frame 0
        let frame = &s.frames[0];
        assert_eq!(frame.trust, FrameTrust::Context);
        assert_eq!(frame.context.valid, MinidumpContextValidity::All);
    }

    {
        // Frame 2 (Found as Frame 1)
        let frame = &s.frames[1];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address2);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame2_sp.value().unwrap() as u32
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame2_fp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }
}

#[tokio::test]
async fn test_frame_pointer_infinite_equality() {
    // Leaf functions on Arm are allowed to not update the stack pointer, so
    // it's valid for the frame pointer analysis to conclude that the stack
    // pointer doesn't change. However we must only provide this allowance
    // to the first stack frame, or else we're vulnerable to infinite loops.
    //
    // One of the CFI tests already checks that we allow the leaf case to work,
    // so here we test that we don't get stuck in an infinite loop for the
    // non-leaf case.
    //
    // This is just a copy-paste of test_frame_pointer except for the line
    // "EVIL INFINITE FRAME POINTER" has been changed from frame2_fp to frame1_fp.
    let mut f = TestFixture::new();
    let mut stack = Section::new();
    stack.start().set_const(0x80000000);

    let return_address1 = 0x50000100u32;
    let return_address2 = 0x50000900u32;
    let frame1_sp = Label::new();
    let frame2_sp = Label::new();
    let frame0_fp = Label::new();
    let frame1_fp = Label::new();
    let frame2_fp = Label::new();

    stack = stack
        // frame 0
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame0_fp) // next fp will point to the next value
        .D32(&frame0_fp) // EVIL INFINITE FRAME POINTER
        .D32(return_address1) // save current link register
        .mark(&frame1_sp)
        // frame 1
        .append_repeated(0, 32) // space
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address
        .mark(&frame1_fp)
        .D32(&frame2_fp)
        .D32(return_address2)
        .mark(&frame2_sp)
        // frame 2
        .append_repeated(0, 32) // Whatever values on the stack.
        .D32(0x0000000D) // junk that's not
        .D32(0xF0000000) // a return address.
        .mark(&frame2_fp)
        .D32(0)
        .D32(0);

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("lr", return_address1);
    f.raw.set_register("fp", frame0_fp.value().unwrap() as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as u32);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // Frame 0
        let frame = &s.frames[0];
        assert_eq!(frame.trust, FrameTrust::Context);
        assert_eq!(frame.context.valid, MinidumpContextValidity::All);
    }

    {
        // Frame 1 (a messed up combination of frame 0 and 1)
        let frame = &s.frames[1];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address1);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as u32
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame0_fp.value().unwrap() as u32
            );
        } else {
            unreachable!();
        }
    }

    // Never get to frame 2, alas!
}

const CALLEE_SAVE_REGS: &[&str] = &["pc", "sp", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "fp"];

fn init_cfi_state() -> (TestFixture, Section, CONTEXT_ARM, MinidumpContextValidity) {
    let mut f = TestFixture::new();
    let symbols = [
        // The youngest frame's function.
        "FUNC 4000 1000 10 enchiridion\n",
        // Initially, nothing has been pushed on the stack,
        // and the return address is still in the link register.
        "STACK CFI INIT 4000 100 .cfa: sp .ra: lr\n",
        // Push r4, the frame pointer, and the link register.
        "STACK CFI 4001 .cfa: sp 12 + r4: .cfa 12 - ^",
        " r11: .cfa 8 - ^ .ra: .cfa 4 - ^\n",
        // Save r4..r7 in r0..r3: verify that we populate
        // the youngest frame with all the values we have.
        "STACK CFI 4002 r4: r0 r5: r1 r6: r2 r7: r3\n",
        // Restore r4..r7. Save the non-callee-saves register r1.
        "STACK CFI 4003 .cfa: sp 16 + r1: .cfa 16 - ^",
        " r4: r4 r5: r5 r6: r6 r7: r7\n",
        // Move the .cfa back four bytes, to point at the return
        // address, and restore the sp explicitly.
        "STACK CFI 4005 .cfa: sp 12 + r1: .cfa 12 - ^",
        " r11: .cfa 4 - ^ .ra: .cfa ^ sp: .cfa 4 +\n",
        // Recover the PC explicitly from a new stack slot;
        // provide garbage for the .ra.
        "STACK CFI 4006 .cfa: sp 16 + pc: .cfa 16 - ^\n",
        // The calling function.
        "FUNC 5000 1000 10 epictetus\n",
        // Mark it as end of stack.
        "STACK CFI INIT 5000 1000 .cfa: 0 .ra: 0\n",
        // A function whose CFI makes the stack pointer
        // go backwards.
        "FUNC 6000 1000 20 palinal\n",
        "STACK CFI INIT 6000 1000 .cfa: sp 4 - .ra: lr\n",
        // A function with CFI expressions that can't be
        // evaluated.
        "FUNC 7000 1000 20 rhetorical\n",
        "STACK CFI INIT 7000 1000 .cfa: moot .ra: ambiguous\n",
    ];
    f.add_symbols(String::from("module1"), symbols.concat());

    f.raw.set_register("pc", 0x40005510);
    f.raw.set_register("sp", 0x80000000);
    f.raw.set_register("fp", 0x8112e110);
    f.raw.iregs[4] = 0xb5d55e68;
    f.raw.iregs[5] = 0xebd134f3;
    f.raw.iregs[6] = 0xa31e74bc;
    f.raw.iregs[7] = 0x2dcb16b3;
    f.raw.iregs[8] = 0x2ada2137;
    f.raw.iregs[9] = 0xbbbb557d;
    f.raw.iregs[10] = 0x48bf8ca7;

    let raw_valid = MinidumpContextValidity::All;

    let expected = f.raw.clone();
    let expected_regs = CALLEE_SAVE_REGS;
    let expected_valid = MinidumpContextValidity::Some(expected_regs.iter().copied().collect());

    let stack = Section::new();
    stack
        .start()
        .set_const(f.raw.get_register("sp", &raw_valid).unwrap() as u64);

    (f, stack, expected, expected_valid)
}

async fn check_cfi(
    f: TestFixture,
    stack: Section,
    expected: CONTEXT_ARM,
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

            if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
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
    let (mut f, mut stack, expected, expected_valid) = init_cfi_state();

    stack = stack.append_repeated(0, 120);

    f.raw.set_register("pc", 0x40004000);
    f.raw.set_register("lr", 0x40005510);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4001() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0xb5d55e68) // saved r4
        .D32(0x8112e110) // saved fp
        .D32(0x40005510) // return address
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    f.raw.set_register("pc", 0x40004001);
    f.raw.iregs[4] = 0x635adc9f;
    f.raw.set_register("fp", 0xbe145fc4);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4002() {
    let (mut f, mut stack, mut expected, expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0xfb81ff3d) // no longer saved r4
        .D32(0x8112e110) // saved fp
        .D32(0x40005510) // return address
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    f.raw.set_register("pc", 0x40004002);
    f.raw.iregs[0] = 0xb5d55e68; // saved r4
    f.raw.iregs[1] = 0xebd134f3; // saved r5
    f.raw.iregs[2] = 0xa31e74bc; // saved r6
    f.raw.iregs[3] = 0x2dcb16b3; // saved r7
    f.raw.iregs[4] = 0xfdd35466; // distinct callee r4
    f.raw.iregs[5] = 0xf18c946c; // distinct callee r5
    f.raw.iregs[6] = 0xac2079e8; // distinct callee r6
    f.raw.iregs[7] = 0xa449829f; // distinct callee r7
    f.raw.set_register("fp", 0xbe145fc4);

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4003() {
    let (mut f, mut stack, mut expected, mut expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0x48c8dd5a) // saved r1 (even though it's not callee-saves)
        .D32(0xcb78040e) // no longer saved r4
        .D32(0x8112e110) // saved fp
        .D32(0x40005510) // return address
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    expected.iregs[1] = 0x48c8dd5a;
    if let MinidumpContextValidity::Some(ref mut which) = expected_valid {
        which.insert("r1");
    } else {
        unreachable!();
    }

    f.raw.set_register("pc", 0x40004003);
    f.raw.iregs[1] = 0xfb756319;

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4004() {
    // Should be the same as 4003
    let (mut f, mut stack, mut expected, mut expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0x48c8dd5a) // saved r1 (even though it's not callee-saves)
        .D32(0xcb78040e) // no longer saved r4
        .D32(0x8112e110) // saved fp
        .D32(0x40005510) // return address
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    expected.iregs[1] = 0x48c8dd5a;
    if let MinidumpContextValidity::Some(ref mut which) = expected_valid {
        which.insert("r1");
    } else {
        unreachable!();
    }

    f.raw.set_register("pc", 0x40004004);
    f.raw.iregs[1] = 0xfb756319;

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4005() {
    let (mut f, mut stack, mut expected, mut expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0x48c8dd5a) // saved r1 (even though it's not callee-saves)
        .D32(0xf013f841) // no longer saved r4
        .D32(0x8112e110) // saved fp
        .D32(0x40005510) // return address
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    expected.iregs[1] = 0x48c8dd5a;
    if let MinidumpContextValidity::Some(ref mut which) = expected_valid {
        which.insert("r1");
    } else {
        unreachable!();
    }

    f.raw.set_register("pc", 0x40004005);
    f.raw.iregs[1] = 0xfb756319;

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_at_4006() {
    // Here we provide an explicit rule for the PC, and have the saved .ra be
    // bogus.

    let (mut f, mut stack, mut expected, mut expected_valid) = init_cfi_state();

    let frame1_sp = Label::new();
    stack = stack
        .D32(0x40005510) // saved pc
        .D32(0x48c8dd5a) // saved r1 (even though it's not callee-saves)
        .D32(0xf013f841) // no longer saved r4
        .D32(0x8112e110) // saved fp
        .D32(0xf8d15783) // .ra rule recovers this, which is garbage
        .mark(&frame1_sp)
        .append_repeated(0, 120);

    expected.set_register("sp", frame1_sp.value().unwrap() as u32);
    expected.iregs[1] = 0x48c8dd5a;
    if let MinidumpContextValidity::Some(ref mut which) = expected_valid {
        which.insert("r1");
    } else {
        unreachable!();
    }

    f.raw.set_register("pc", 0x40004006);
    f.raw.iregs[1] = 0xfb756319;

    check_cfi(f, stack, expected, expected_valid).await;
}

#[tokio::test]
async fn test_cfi_reject_backwards() {
    // Check that we reject rules that would cause the stack pointer to
    // move in the wrong direction.

    let (mut f, mut stack, _expected, _expected_valid) = init_cfi_state();

    stack = stack.append_repeated(0, 120);

    f.raw.set_register("pc", 0x40006000);
    f.raw.set_register("sp", 0x80000000);
    f.raw.set_register("lr", 0x40005510);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);
}

#[tokio::test]
async fn test_cfi_reject_bad_exprs() {
    // Check that we reject rules whose expressions' evaluation fails.

    let (mut f, mut stack, _expected, _expected_valid) = init_cfi_state();

    stack = stack.append_repeated(0, 120);

    f.raw.set_register("pc", 0x40007000);
    f.raw.set_register("sp", 0x80000000);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);
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

    f.raw.set_register("pc", 0x7a100000);
    f.raw.set_register("fp", bad_frame_ptr);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as Pointer);
    f.raw.set_register("lr", 0x7b302000);

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

    f.raw.set_register("pc", 0x7a100000);
    f.raw.set_register("fp", bad_frame_ptr as u32);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as Pointer);
    f.raw.set_register("lr", 0x7b302000);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 1);

    // As long as we don't panic, we're good!
}

#[tokio::test]
async fn test_frame_pointer_barely_no_overflow() {
    // This is a simple frame pointer test but with the all the values pushed
    // as close to the upper memory boundary as possible, to confirm that
    // our code doesn't randomly overflow *AND* isn't overzealous in
    // its overflow guards.

    let mut f = TestFixture::new();
    let mut stack = Section::new();

    type Pointer = u32;
    let stack_max: Pointer = Pointer::MAX;
    let pointer_size: Pointer = std::mem::size_of::<Pointer>() as Pointer;
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

    f.raw.set_register("pc", 0x7a100000);
    f.raw
        .set_register("fp", frame0_fp.value().unwrap() as Pointer);
    f.raw
        .set_register("sp", stack.start().value().unwrap() as Pointer);
    f.raw.set_register("lr", return_address);

    let s = f.walk_stack(stack).await;
    assert_eq!(s.frames.len(), 2);

    {
        // Frame 0
        let frame = &s.frames[0];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::Context);
        assert_eq!(frame.context.valid, MinidumpContextValidity::All);

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame0_fp.value().unwrap() as Pointer
            );
        } else {
            unreachable!();
        }
    }

    {
        // Frame 1
        let frame = &s.frames[1];
        let valid = &frame.context.valid;
        assert_eq!(frame.trust, FrameTrust::FramePointer);
        if let MinidumpContextValidity::Some(ref which) = valid {
            assert_eq!(which.len(), 3);
        } else {
            unreachable!();
        }

        if let MinidumpRawContext::Arm(ctx) = &frame.context.raw {
            assert_eq!(ctx.get_register("pc", valid).unwrap(), return_address);
            assert_eq!(
                ctx.get_register("sp", valid).unwrap(),
                frame1_sp.value().unwrap() as Pointer
            );
            assert_eq!(
                ctx.get_register("fp", valid).unwrap(),
                frame1_fp.value().unwrap() as Pointer
            );
        } else {
            unreachable!();
        }
    }
}
