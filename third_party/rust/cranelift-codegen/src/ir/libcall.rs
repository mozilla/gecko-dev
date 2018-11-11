//! Naming well-known routines in the runtime library.

use ir::{
    types, AbiParam, ArgumentPurpose, ExtFuncData, ExternalName, FuncRef, Function, Inst, Opcode,
    Signature, Type,
};
use isa::{CallConv, RegUnit, TargetIsa};
use std::fmt;
use std::str::FromStr;

/// The name of a runtime library routine.
///
/// Runtime library calls are generated for Cranelift IR instructions that don't have an equivalent
/// ISA instruction or an easy macro expansion. A `LibCall` is used as a well-known name to refer to
/// the runtime library routine. This way, Cranelift doesn't have to know about the naming
/// convention in the embedding VM's runtime library.
///
/// This list is likely to grow over time.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum LibCall {
    /// probe for stack overflow. These are emitted for functions which need
    /// when the `probestack_enabled` setting is true.
    Probestack,
    /// ceil.f32
    CeilF32,
    /// ceil.f64
    CeilF64,
    /// floor.f32
    FloorF32,
    /// floor.f64
    FloorF64,
    /// trunc.f32
    TruncF32,
    /// frunc.f64
    TruncF64,
    /// nearest.f32
    NearestF32,
    /// nearest.f64
    NearestF64,
    /// libc.memcpy
    Memcpy,
    /// libc.memset
    Memset,
    /// libc.memmove
    Memmove,
}

impl fmt::Display for LibCall {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

impl FromStr for LibCall {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Probestack" => Ok(LibCall::Probestack),
            "CeilF32" => Ok(LibCall::CeilF32),
            "CeilF64" => Ok(LibCall::CeilF64),
            "FloorF32" => Ok(LibCall::FloorF32),
            "FloorF64" => Ok(LibCall::FloorF64),
            "TruncF32" => Ok(LibCall::TruncF32),
            "TruncF64" => Ok(LibCall::TruncF64),
            "NearestF32" => Ok(LibCall::NearestF32),
            "NearestF64" => Ok(LibCall::NearestF64),
            "Memcpy" => Ok(LibCall::Memcpy),
            "Memset" => Ok(LibCall::Memset),
            "Memmove" => Ok(LibCall::Memmove),
            _ => Err(()),
        }
    }
}

impl LibCall {
    /// Get the well-known library call name to use as a replacement for an instruction with the
    /// given opcode and controlling type variable.
    ///
    /// Returns `None` if no well-known library routine name exists for that instruction.
    pub fn for_inst(opcode: Opcode, ctrl_type: Type) -> Option<Self> {
        Some(match ctrl_type {
            types::F32 => match opcode {
                Opcode::Ceil => LibCall::CeilF32,
                Opcode::Floor => LibCall::FloorF32,
                Opcode::Trunc => LibCall::TruncF32,
                Opcode::Nearest => LibCall::NearestF32,
                _ => return None,
            },
            types::F64 => match opcode {
                Opcode::Ceil => LibCall::CeilF64,
                Opcode::Floor => LibCall::FloorF64,
                Opcode::Trunc => LibCall::TruncF64,
                Opcode::Nearest => LibCall::NearestF64,
                _ => return None,
            },
            _ => return None,
        })
    }
}

/// Get a function reference for `libcall` in `func`, following the signature
/// for `inst`.
///
/// If there is an existing reference, use it, otherwise make a new one.
pub fn get_libcall_funcref(
    libcall: LibCall,
    func: &mut Function,
    inst: Inst,
    isa: &TargetIsa,
) -> FuncRef {
    find_funcref(libcall, func).unwrap_or_else(|| make_funcref_for_inst(libcall, func, inst, isa))
}

/// Get a function reference for the probestack function in `func`.
///
/// If there is an existing reference, use it, otherwise make a new one.
pub fn get_probestack_funcref(
    func: &mut Function,
    reg_type: Type,
    arg_reg: RegUnit,
    isa: &TargetIsa,
) -> FuncRef {
    find_funcref(LibCall::Probestack, func)
        .unwrap_or_else(|| make_funcref_for_probestack(func, reg_type, arg_reg, isa))
}

/// Get the existing function reference for `libcall` in `func` if it exists.
fn find_funcref(libcall: LibCall, func: &Function) -> Option<FuncRef> {
    // We're assuming that all libcall function decls are at the end.
    // If we get this wrong, worst case we'll have duplicate libcall decls which is harmless.
    for (fref, func_data) in func.dfg.ext_funcs.iter().rev() {
        match func_data.name {
            ExternalName::LibCall(lc) => {
                if lc == libcall {
                    return Some(fref);
                }
            }
            _ => break,
        }
    }
    None
}

/// Create a funcref for `LibCall::Probestack`.
fn make_funcref_for_probestack(
    func: &mut Function,
    reg_type: Type,
    arg_reg: RegUnit,
    isa: &TargetIsa,
) -> FuncRef {
    let mut sig = Signature::new(CallConv::Probestack);
    let rax = AbiParam::special_reg(reg_type, ArgumentPurpose::Normal, arg_reg);
    sig.params.push(rax);
    if !isa.flags().probestack_func_adjusts_sp() {
        sig.returns.push(rax);
    }
    make_funcref(LibCall::Probestack, func, sig, isa)
}

/// Create a funcref for `libcall` with a signature matching `inst`.
fn make_funcref_for_inst(
    libcall: LibCall,
    func: &mut Function,
    inst: Inst,
    isa: &TargetIsa,
) -> FuncRef {
    let mut sig = Signature::new(isa.default_call_conv());
    for &v in func.dfg.inst_args(inst) {
        sig.params.push(AbiParam::new(func.dfg.value_type(v)));
    }
    for &v in func.dfg.inst_results(inst) {
        sig.returns.push(AbiParam::new(func.dfg.value_type(v)));
    }

    make_funcref(libcall, func, sig, isa)
}

/// Create a funcref for `libcall`.
fn make_funcref(libcall: LibCall, func: &mut Function, sig: Signature, isa: &TargetIsa) -> FuncRef {
    let sigref = func.import_signature(sig);

    func.import_function(ExtFuncData {
        name: ExternalName::LibCall(libcall),
        signature: sigref,
        colocated: isa.flags().colocated_libcalls(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::string::ToString;

    #[test]
    fn display() {
        assert_eq!(LibCall::CeilF32.to_string(), "CeilF32");
        assert_eq!(LibCall::NearestF64.to_string(), "NearestF64");
    }

    #[test]
    fn parsing() {
        assert_eq!("FloorF32".parse(), Ok(LibCall::FloorF32));
    }
}
