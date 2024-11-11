//! Implementation of emitting DWARF debugging information for `*.wat` files.
//!
//! This is intended to be relatively simple but the goal is to enable emission
//! of DWARF sections which point back to the original `*.wat` file itself. This
//! enables debuggers like LLDB to debug `*.wat` files without necessarily
//! built-in knowledge of WebAssembly itself.
//!
//! Overall I was curious on weekend and decided to implement this. It's an
//! off-by-default crate feature and an off-by-default runtime feature of this
//! crate. Hopefully doesn't carry too much complexity with it while still being
//! easy/fun to play around with.

use crate::core::binary::{EncodeOptions, Encoder, GenerateDwarf, Names, RecOrType};
use crate::core::{InnerTypeKind, Local, ValType};
use crate::token::Span;
use gimli::write::{
    self, Address, AttributeValue, DwarfUnit, Expression, FileId, LineProgram, LineString,
    Sections, UnitEntryId, Writer,
};
use gimli::{Encoding, Format, LineEncoding, LittleEndian};
use std::cmp::Ordering;
use std::collections::HashMap;
use std::path::Path;

pub struct Dwarf<'a> {
    // Metadata configured at `Dwarf` creation time
    func_names: HashMap<u32, &'a str>,
    func_imports: u32,
    contents: &'a str,
    file_id: FileId,
    style: GenerateDwarf,
    types: &'a [RecOrType<'a>],

    dwarf: DwarfUnit,

    // Next function index when `start_func` is called
    next_func: u32,

    // Current `DW_TAG_subprogram` that's being built as part of `start_func`.
    cur_subprogram: Option<UnitEntryId>,
    cur_subprogram_instrs: usize,

    // Code-section-relative offset of the start of every function. Filled in
    // as part of `end_func`.
    sym_offsets: Vec<usize>,

    // Metadata tracking what the line/column information was at the specified
    // last offset.
    line: u64,
    column: u64,
    last_offset: usize,

    i32_ty: Option<UnitEntryId>,
    i64_ty: Option<UnitEntryId>,
    f32_ty: Option<UnitEntryId>,
    f64_ty: Option<UnitEntryId>,
}

impl<'a> Dwarf<'a> {
    /// Creates a new `Dwarf` from the specified configuration.
    ///
    /// * `func_imports` - the number of imported functions in this module, or
    ///   the index of the first defined function.
    /// * `opts` - encoding options, namely where whether DWARF is to be emitted
    ///   is configured.
    /// * `names` - the `name` custom section for this module, used to name
    ///   functions in DWARF.
    pub fn new(
        func_imports: u32,
        opts: &EncodeOptions<'a>,
        names: &Names<'a>,
        types: &'a [RecOrType<'a>],
    ) -> Option<Dwarf<'a>> {
        // This is a load-bearing `?` which notably short-circuits all DWARF
        // machinery entirely if this was not enabled at runtime.
        let (file, contents, style) = opts.dwarf_info?;
        let file = file.to_str()?;

        // Configure some initial `gimli::write` context.
        let encoding = Encoding {
            address_size: 4,
            format: Format::Dwarf32,
            version: 5,
        };
        let mut dwarf = DwarfUnit::new(encoding);
        let (comp_dir, comp_file) = match (
            Path::new(file).parent().and_then(|s| s.to_str()),
            Path::new(file).file_name().and_then(|s| s.to_str()),
        ) {
            (Some(parent), Some(file_name)) if !parent.is_empty() => (parent, file_name),
            _ => (".", file),
        };
        let comp_dir_ref = dwarf.strings.add(comp_dir);
        let comp_file_ref = dwarf.strings.add(comp_file);
        dwarf.unit.line_program = LineProgram::new(
            encoding,
            LineEncoding::default(),
            LineString::StringRef(comp_dir_ref),
            LineString::StringRef(comp_file_ref),
            None,
        );
        let dir_id = dwarf.unit.line_program.default_directory();
        let file_id =
            dwarf
                .unit
                .line_program
                .add_file(LineString::StringRef(comp_file_ref), dir_id, None);

        // Configure a single compilation unit which encompasses the entire code
        // section. The code section isn't fully known at this point so only a
        // "low pc" is emitted here.
        let root = dwarf.unit.root();
        let cu = dwarf.unit.get_mut(root);
        cu.set(
            gimli::DW_AT_producer,
            AttributeValue::String(format!("wast {}", env!("CARGO_PKG_VERSION")).into_bytes()),
        );
        cu.set(
            gimli::DW_AT_language,
            // Technically this should be something like wasm or wat but that
            // doesn't exist so fill in something here.
            AttributeValue::Language(gimli::DW_LANG_C),
        );
        cu.set(gimli::DW_AT_name, AttributeValue::StringRef(comp_file_ref));
        cu.set(
            gimli::DW_AT_comp_dir,
            AttributeValue::StringRef(comp_dir_ref),
        );
        cu.set(gimli::DW_AT_low_pc, AttributeValue::Data4(0));

        // Build a lookup table for defined function index to its name.
        let mut func_names = HashMap::new();
        for (idx, name) in names.funcs.iter() {
            func_names.insert(*idx, *name);
        }

        // Offsets pointing to newlines are considered internally as the 0th
        // column of the next line, so handle the case that the contents start
        // with a newline.
        let (line, column) = if contents.starts_with("\n") {
            (2, 0)
        } else {
            (1, 1)
        };
        Some(Dwarf {
            dwarf,
            style,
            next_func: func_imports,
            func_imports,
            sym_offsets: Vec::new(),
            contents,
            line,
            column,
            last_offset: 0,
            file_id,
            cur_subprogram: None,
            cur_subprogram_instrs: 0,
            func_names,
            i32_ty: None,
            i64_ty: None,
            f32_ty: None,
            f64_ty: None,
            types,
        })
    }

    /// Start emitting a new function defined at `span`.
    ///
    /// This will start a new line program for this function and additionally
    /// configure a new `DW_TAG_subprogram` for this new function.
    pub fn start_func(&mut self, span: Span, ty: u32, locals: &[Local<'_>]) {
        self.change_linecol(span);
        let addr = Address::Symbol {
            symbol: (self.next_func - self.func_imports) as usize,
            addend: 0,
        };
        self.dwarf.unit.line_program.begin_sequence(Some(addr));

        let root = self.dwarf.unit.root();
        let subprogram = self.dwarf.unit.add(root, gimli::DW_TAG_subprogram);
        let entry = self.dwarf.unit.get_mut(subprogram);
        let fallback = format!("wasm-function[{}]", self.next_func);
        let func_name = self
            .func_names
            .get(&self.next_func)
            .copied()
            .unwrap_or(&fallback);
        entry.set(gimli::DW_AT_name, AttributeValue::String(func_name.into()));
        entry.set(
            gimli::DW_AT_decl_file,
            AttributeValue::FileIndex(Some(self.file_id)),
        );
        entry.set(gimli::DW_AT_decl_line, AttributeValue::Udata(self.line));
        entry.set(gimli::DW_AT_decl_column, AttributeValue::Udata(self.column));
        entry.set(gimli::DW_AT_external, AttributeValue::FlagPresent);
        entry.set(gimli::DW_AT_low_pc, AttributeValue::Address(addr));

        if let GenerateDwarf::Full = self.style {
            self.add_func_params_and_locals(subprogram, ty, locals);
        }

        self.cur_subprogram = Some(subprogram);
        self.cur_subprogram_instrs = 0;
        self.next_func += 1;
    }

    /// Adds `DW_TAG_formal_parameter` and `DW_TAG_variable` for all locals
    /// (which are both params and function-defined locals).
    ///
    /// This is pretty simple in that the expression for the location of
    /// these variables is constant, it's just "it's the local", and it spans
    /// the entire function.
    fn add_func_params_and_locals(
        &mut self,
        subprogram: UnitEntryId,
        ty: u32,
        locals: &[Local<'_>],
    ) {
        // Iterate through `self.types` which is what was encoded into the
        // module and find the function type which gives access to the
        // parameters which gives access to their types.
        let ty = self
            .types
            .iter()
            .flat_map(|t| match t {
                RecOrType::Type(t) => std::slice::from_ref(*t),
                RecOrType::Rec(r) => &r.types,
            })
            .nth(ty as usize);
        let ty = match ty.map(|t| &t.def.kind) {
            Some(InnerTypeKind::Func(ty)) => ty,
            _ => return,
        };

        let mut local_idx = 0;
        for (_, _, ty) in ty.params.iter() {
            self.local(local_idx, subprogram, gimli::DW_TAG_formal_parameter, ty);
            local_idx += 1;
        }

        for local in locals {
            self.local(local_idx, subprogram, gimli::DW_TAG_variable, &local.ty);
            local_idx += 1;
        }
    }

    /// Attempts to define a local variable within `subprogram` with the `ty`
    /// given.
    ///
    /// This does nothing if `ty` can't be represented in DWARF.
    fn local(&mut self, local: u32, subprogram: UnitEntryId, tag: gimli::DwTag, ty: &ValType<'_>) {
        let ty = match self.val_type_to_dwarf_type(ty) {
            Some(ty) => ty,
            None => return,
        };

        let param = self.dwarf.unit.add(subprogram, tag);
        let entry = self.dwarf.unit.get_mut(param);
        entry.set(
            gimli::DW_AT_name,
            AttributeValue::String(format!("local{local}").into()),
        );

        let mut loc = Expression::new();
        loc.op_wasm_local(local);
        loc.op(gimli::DW_OP_stack_value);
        entry.set(gimli::DW_AT_location, AttributeValue::Exprloc(loc));
        entry.set(gimli::DW_AT_type, AttributeValue::UnitRef(ty));
    }

    fn val_type_to_dwarf_type(&mut self, ty: &ValType<'_>) -> Option<UnitEntryId> {
        match ty {
            ValType::I32 => Some(self.i32_ty()),
            ValType::I64 => Some(self.i64_ty()),
            ValType::F32 => Some(self.f32_ty()),
            ValType::F64 => Some(self.f64_ty()),
            // TODO: make a C union of sorts or something like that to
            // represent v128 as an array-of-lanes or u128 or something like
            // that.
            ValType::V128 => None,
            // Not much that can be done about reference types without actually
            // knowing what the engine does, this probably needs an addition to
            // DWARF itself to represent this.
            ValType::Ref(_) => None,
        }
    }

    /// Emit an instruction which starts at `offset` and is defined at `span`.
    ///
    /// Note that `offset` is code-section-relative.
    pub fn instr(&mut self, offset: usize, span: Span) {
        self.change_linecol(span);
        let offset = u64::try_from(offset).unwrap();

        let mut changed = false;
        let row = self.dwarf.unit.line_program.row();
        set(&mut row.address_offset, offset, &mut changed);
        set(&mut row.line, self.line, &mut changed);
        set(&mut row.column, self.column, &mut changed);
        set(&mut row.file, self.file_id, &mut changed);
        set(&mut row.is_statement, true, &mut changed);
        set(
            &mut row.prologue_end,
            self.cur_subprogram_instrs == 0,
            &mut changed,
        );

        if changed {
            self.dwarf.unit.line_program.generate_row();
        }
        self.cur_subprogram_instrs += 1;

        fn set<T: PartialEq>(slot: &mut T, val: T, changed: &mut bool) {
            if *slot != val {
                *slot = val;
                *changed = true;
            }
        }
    }

    /// Change `self.line` and `self.column` to be appropriate for the offset
    /// in `span`.
    ///
    /// This will incrementally move from `self.last_offset` to `span.offset()`
    /// and update line/column information as we go. It's assumed that this is
    /// more efficient than a precalculate-all-the-positions-for-each-byte
    /// approach since that would require a great deal of memory to store a
    /// line/col for all bytes in the input string. It's also assumed that most
    /// `span` adjustments are minor as it's between instructions in a function
    /// which are frequently close together. Whether or not this assumption
    /// pans out is yet to be seen.
    fn change_linecol(&mut self, span: Span) {
        let offset = span.offset();

        loop {
            match self.last_offset.cmp(&offset) {
                Ordering::Less => {
                    let haystack = self.contents[self.last_offset + 1..].as_bytes();
                    let next_newline = match memchr::memchr_iter(b'\n', haystack).next() {
                        Some(pos) => pos + self.last_offset + 1,
                        None => break,
                    };
                    if next_newline > offset {
                        break;
                    } else {
                        self.line += 1;
                        self.column = 0;
                        self.last_offset = next_newline;
                    }
                }
                Ordering::Greater => {
                    let haystack = self.contents[..self.last_offset].as_bytes();
                    match memchr::memchr_iter(b'\n', haystack).next_back() {
                        Some(prev_newline) => {
                            if self.column == 0 {
                                self.line -= 1;
                            }
                            self.column = 0;
                            self.last_offset = prev_newline;
                        }
                        None => {
                            self.line = 1;
                            self.column = 1;
                            self.last_offset = 0;
                        }
                    }
                }
                Ordering::Equal => break,
            }
        }

        match self.last_offset.cmp(&offset) {
            Ordering::Less => {
                self.column += (offset - self.last_offset) as u64;
            }
            Ordering::Greater => {
                self.column -= (self.last_offset - offset) as u64;
            }
            Ordering::Equal => {}
        }
        self.last_offset = offset;
    }

    /// Completes emission of the latest function.
    ///
    /// The latest function took `func_size` bytes to encode and the current end
    /// of the code section, after the function was appended, is
    /// `code_section_end`.
    pub fn end_func(&mut self, func_size: usize, code_section_end: usize) {
        // Add a final row corresponding to the final `end` instruction in the
        // function to ensure there's something for all bytecodes.
        let row = self.dwarf.unit.line_program.row();
        row.address_offset = (func_size - 1) as u64;
        self.dwarf.unit.line_program.generate_row();

        // This function's symbol is relative to the start of the function
        // itself. Functions are encoded as a leb-size-of-function then the
        // function itself, so to account for the size of the
        // leb-size-of-function we calculate the function start as the current
        // end of the code section minus the size of the function's bytes.
        self.sym_offsets.push(code_section_end - func_size);

        // The line program is relative to the start address, so only the
        // function's size is used here.
        self.dwarf
            .unit
            .line_program
            .end_sequence(u64::try_from(func_size).unwrap());

        // The high PC value here is relative to `DW_AT_low_pc`, so it's the
        // size of the function.
        let entry = self.dwarf.unit.get_mut(self.cur_subprogram.take().unwrap());
        entry.set(
            gimli::DW_AT_high_pc,
            AttributeValue::Data4(func_size as u32),
        );
    }

    pub fn set_code_section_size(&mut self, size: usize) {
        let root = self.dwarf.unit.root();
        let entry = self.dwarf.unit.get_mut(root);
        entry.set(gimli::DW_AT_high_pc, AttributeValue::Data4(size as u32));
    }

    pub fn emit(&mut self, dst: &mut Encoder<'_>) {
        let mut sections = Sections::new(DwarfWriter {
            sym_offsets: &self.sym_offsets,
            bytes: Vec::new(),
        });
        self.dwarf.write(&mut sections).unwrap();

        sections
            .for_each(|id, writer| {
                if !writer.bytes.is_empty() {
                    dst.wasm.section(&wasm_encoder::CustomSection {
                        name: id.name().into(),
                        data: (&writer.bytes).into(),
                    });
                }
                Ok::<_, std::convert::Infallible>(())
            })
            .unwrap();
    }

    fn i32_ty(&mut self) -> UnitEntryId {
        if self.i32_ty.is_none() {
            self.i32_ty = Some(self.mk_primitive("i32", 4, gimli::DW_ATE_signed));
        }
        self.i32_ty.unwrap()
    }

    fn i64_ty(&mut self) -> UnitEntryId {
        if self.i64_ty.is_none() {
            self.i64_ty = Some(self.mk_primitive("i64", 8, gimli::DW_ATE_signed));
        }
        self.i64_ty.unwrap()
    }

    fn f32_ty(&mut self) -> UnitEntryId {
        if self.f32_ty.is_none() {
            self.f32_ty = Some(self.mk_primitive("f32", 4, gimli::DW_ATE_float));
        }
        self.f32_ty.unwrap()
    }

    fn f64_ty(&mut self) -> UnitEntryId {
        if self.f64_ty.is_none() {
            self.f64_ty = Some(self.mk_primitive("f64", 8, gimli::DW_ATE_float));
        }
        self.f64_ty.unwrap()
    }

    fn mk_primitive(&mut self, name: &str, byte_size: u8, encoding: gimli::DwAte) -> UnitEntryId {
        let name = self.dwarf.strings.add(name);
        let root = self.dwarf.unit.root();
        let ty = self.dwarf.unit.add(root, gimli::DW_TAG_base_type);
        let entry = self.dwarf.unit.get_mut(ty);
        entry.set(gimli::DW_AT_name, AttributeValue::StringRef(name));
        entry.set(gimli::DW_AT_byte_size, AttributeValue::Data1(byte_size));
        entry.set(gimli::DW_AT_encoding, AttributeValue::Encoding(encoding));
        ty
    }
}

#[derive(Clone)]
struct DwarfWriter<'a> {
    sym_offsets: &'a [usize],
    bytes: Vec<u8>,
}

impl Writer for DwarfWriter<'_> {
    type Endian = LittleEndian;

    fn endian(&self) -> Self::Endian {
        LittleEndian
    }
    fn len(&self) -> usize {
        self.bytes.len()
    }
    fn write(&mut self, bytes: &[u8]) -> write::Result<()> {
        self.bytes.extend_from_slice(bytes);
        Ok(())
    }
    fn write_at(&mut self, offset: usize, bytes: &[u8]) -> write::Result<()> {
        self.bytes[offset..][..bytes.len()].copy_from_slice(bytes);
        Ok(())
    }
    fn write_address(&mut self, address: Address, size: u8) -> write::Result<()> {
        match address {
            Address::Constant(val) => self.write_udata(val, size),
            Address::Symbol { symbol, addend } => {
                assert_eq!(addend, 0);
                let offset = self.sym_offsets[symbol];
                self.write_udata(offset as u64, size)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Dwarf, EncodeOptions, GenerateDwarf};
    use crate::token::Span;
    use rand::rngs::SmallRng;
    use rand::{Rng, SeedableRng};

    fn linecol_test(contents: &str) {
        let mut dwarf = Dwarf::new(
            0,
            EncodeOptions::default().dwarf("foo.wat".as_ref(), contents, GenerateDwarf::Lines),
            &Default::default(),
            &[],
        )
        .unwrap();

        // Print some debugging information in case someone's debugging this
        // test
        let mut offset = 0;
        for (i, line) in contents.lines().enumerate() {
            println!(
                "line {:2} is at {:2} .. {:2}",
                i + 1,
                offset,
                offset + line.len()
            );
            offset += line.len() + 1;
        }
        println!("");

        // Precalculate (line, col) for all characters, assumed to all be one
        // byte here.
        let mut precalculated_linecols = Vec::new();
        let mut line = 1;
        let mut col = 1;
        for c in contents.chars() {
            if c == '\n' {
                line += 1;
                col = 0;
            }
            precalculated_linecols.push((line, col));
            col += 1;
        }

        // Traverse randomly throughout this string and assert that the
        // incremental update matches the precalculated position.
        let mut rand = SmallRng::seed_from_u64(102);
        for _ in 0..1000 {
            let pos = rand.gen_range(0..contents.len());
            dwarf.change_linecol(Span::from_offset(pos));
            let (line, col) = precalculated_linecols[pos];

            assert_eq!(dwarf.line, line, "line mismatch");
            assert_eq!(dwarf.column, col, "column mismatch");
        }
    }

    #[test]
    fn linecol_simple() {
        linecol_test(
            "a

        b
        c (; ... ;)
        d
         e


         f

         fg",
        );
    }

    #[test]
    fn linecol_empty() {
        linecol_test("x");
    }

    #[test]
    fn linecol_start_newline() {
        linecol_test("\nx ab\nyyy \ncc");
    }

    #[test]
    fn linecol_lots_of_newlines() {
        linecol_test("\n\n\n\n");
    }

    #[test]
    fn linecol_interspersed() {
        linecol_test("\na\nb\nc\n");
    }
}
