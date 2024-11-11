#[cfg(feature = "component-model")]
use crate::component::Component;
use crate::core::*;
use crate::encode::Encode;
use crate::token::*;
use crate::Wat;
use std::borrow::Cow;
use std::marker;
#[cfg(feature = "dwarf")]
use std::path::Path;

/// Options that can be specified when encoding a component or a module to
/// customize what the final binary looks like.
///
/// Methods such as [`Module::encode`], [`Wat::encode`], and
/// [`Component::encode`] will use the default options.
#[derive(Default)]
pub struct EncodeOptions<'a> {
    #[cfg(feature = "dwarf")]
    dwarf_info: Option<(&'a Path, &'a str, GenerateDwarf)>,

    _marker: marker::PhantomData<&'a str>,
}

#[cfg(feature = "dwarf")]
mod dwarf;

#[cfg(not(feature = "dwarf"))]
mod dwarf_disabled;
#[cfg(not(feature = "dwarf"))]
use self::dwarf_disabled as dwarf;

/// Configuration of how DWARF debugging information may be generated.
#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub enum GenerateDwarf {
    /// Only generate line tables to map binary offsets back to source
    /// locations.
    Lines,

    /// Generate full debugging information for both line numbers and
    /// variables/locals/operands.
    Full,
}

impl<'a> EncodeOptions<'a> {
    /// Creates a new set of default encoding options.
    pub fn new() -> EncodeOptions<'a> {
        EncodeOptions::default()
    }

    /// Enables emission of DWARF debugging information in the final binary.
    ///
    /// This method will use the `file` specified as the source file for the
    /// `*.wat` file whose `contents` must also be supplied here. These are
    /// used to calculate filenames/line numbers and are referenced from the
    /// generated DWARF.
    #[cfg(feature = "dwarf")]
    pub fn dwarf(&mut self, file: &'a Path, contents: &'a str, style: GenerateDwarf) -> &mut Self {
        self.dwarf_info = Some((file, contents, style));
        self
    }

    /// Encodes the given [`Module`] with these options.
    ///
    /// For more information see [`Module::encode`].
    pub fn encode_module(
        &self,
        module: &mut Module<'_>,
    ) -> std::result::Result<Vec<u8>, crate::Error> {
        module.resolve()?;
        Ok(match &module.kind {
            ModuleKind::Text(fields) => encode(&module.id, &module.name, fields, self),
            ModuleKind::Binary(blobs) => blobs.iter().flat_map(|b| b.iter().cloned()).collect(),
        })
    }

    /// Encodes the given [`Component`] with these options.
    ///
    /// For more information see [`Component::encode`].
    #[cfg(feature = "component-model")]
    pub fn encode_component(
        &self,
        component: &mut Component<'_>,
    ) -> std::result::Result<Vec<u8>, crate::Error> {
        component.resolve()?;
        Ok(crate::component::binary::encode(component, self))
    }

    /// Encodes the given [`Wat`] with these options.
    ///
    /// For more information see [`Wat::encode`].
    pub fn encode_wat(&self, wat: &mut Wat<'_>) -> std::result::Result<Vec<u8>, crate::Error> {
        match wat {
            Wat::Module(m) => self.encode_module(m),
            #[cfg(feature = "component-model")]
            Wat::Component(c) => self.encode_component(c),
            #[cfg(not(feature = "component-model"))]
            Wat::Component(_) => unreachable!(),
        }
    }
}

pub(crate) fn encode(
    module_id: &Option<Id<'_>>,
    module_name: &Option<NameAnnotation<'_>>,
    fields: &[ModuleField<'_>],
    opts: &EncodeOptions,
) -> Vec<u8> {
    use CustomPlace::*;
    use CustomPlaceAnchor::*;

    let mut types = Vec::new();
    let mut imports = Vec::new();
    let mut funcs = Vec::new();
    let mut tables = Vec::new();
    let mut memories = Vec::new();
    let mut globals = Vec::new();
    let mut exports = Vec::new();
    let mut start = Vec::new();
    let mut elem = Vec::new();
    let mut data = Vec::new();
    let mut tags = Vec::new();
    let mut customs = Vec::new();
    for field in fields {
        match field {
            ModuleField::Type(i) => types.push(RecOrType::Type(i)),
            ModuleField::Rec(i) => types.push(RecOrType::Rec(i)),
            ModuleField::Import(i) => imports.push(i),
            ModuleField::Func(i) => funcs.push(i),
            ModuleField::Table(i) => tables.push(i),
            ModuleField::Memory(i) => memories.push(i),
            ModuleField::Global(i) => globals.push(i),
            ModuleField::Export(i) => exports.push(i),
            ModuleField::Start(i) => start.push(i),
            ModuleField::Elem(i) => elem.push(i),
            ModuleField::Data(i) => data.push(i),
            ModuleField::Tag(i) => tags.push(i),
            ModuleField::Custom(i) => customs.push(i),
        }
    }

    let mut e = Encoder {
        wasm: wasm_encoder::Module::new(),
        customs: &customs,
    };

    e.custom_sections(BeforeFirst);

    e.typed_section(&types);
    e.typed_section(&imports);

    let functys = funcs
        .iter()
        .map(|f| FuncSectionTy(&f.ty))
        .collect::<Vec<_>>();
    e.typed_section(&functys);
    e.typed_section(&tables);
    e.typed_section(&memories);
    e.typed_section(&tags);
    e.typed_section(&globals);
    e.typed_section(&exports);
    e.custom_sections(Before(Start));
    if let Some(start) = start.get(0) {
        e.wasm.section(&wasm_encoder::StartSection {
            function_index: start.unwrap_u32(),
        });
    }
    e.custom_sections(After(Start));
    e.typed_section(&elem);
    if needs_data_count(&funcs) {
        e.wasm.section(&wasm_encoder::DataCountSection {
            count: data.len().try_into().unwrap(),
        });
    }

    // Prepare to and emit the code section. This is where DWARF may optionally
    // be emitted depending on configuration settings. Note that `code_section`
    // will internally emit the branch hints section if necessary.
    let names = find_names(module_id, module_name, fields);
    let num_import_funcs = imports
        .iter()
        .filter(|i| matches!(i.item.kind, ItemKind::Func(..)))
        .count() as u32;
    let mut dwarf = dwarf::Dwarf::new(num_import_funcs, opts, &names, &types);
    e.code_section(&funcs, num_import_funcs, dwarf.as_mut());

    e.typed_section(&data);

    if !names.is_empty() {
        e.wasm.section(&names.to_name_section());
    }
    e.custom_sections(AfterLast);
    if let Some(dwarf) = &mut dwarf {
        dwarf.emit(&mut e);
    }

    return e.wasm.finish();

    fn needs_data_count(funcs: &[&crate::core::Func<'_>]) -> bool {
        funcs
            .iter()
            .filter_map(|f| match &f.kind {
                FuncKind::Inline { expression, .. } => Some(expression),
                _ => None,
            })
            .flat_map(|e| e.instrs.iter())
            .any(|i| i.needs_data_count())
    }
}

struct Encoder<'a> {
    wasm: wasm_encoder::Module,
    customs: &'a [&'a Custom<'a>],
}

impl Encoder<'_> {
    fn custom_sections(&mut self, place: CustomPlace) {
        for entry in self.customs.iter() {
            if entry.place() == place {
                entry.encode(&mut self.wasm);
            }
        }
    }

    fn typed_section<T>(&mut self, list: &[T])
    where
        T: SectionItem,
    {
        self.custom_sections(CustomPlace::Before(T::ANCHOR));
        if !list.is_empty() {
            let mut section = T::Section::default();
            for item in list {
                item.encode(&mut section);
            }
            self.wasm.section(&section);
        }
        self.custom_sections(CustomPlace::After(T::ANCHOR));
    }

    /// Encodes the code section of a wasm module module while additionally
    /// handling the branch hinting proposal.
    ///
    /// The branch hinting proposal requires to encode the offsets of the
    /// instructions relative from the beginning of the function. Here we encode
    /// each instruction and we save its offset. If needed, we use this
    /// information to build the branch hint section and insert it before the
    /// code section.
    ///
    /// The `list` provided is the list of functions that are emitted into the
    /// code section. The `func_index` provided is the initial index of defined
    /// functions, so it's the count of imported functions. The `dwarf` field is
    /// optionally used to track debugging information.
    fn code_section<'a>(
        &'a mut self,
        list: &[&'a Func<'_>],
        mut func_index: u32,
        mut dwarf: Option<&mut dwarf::Dwarf>,
    ) {
        self.custom_sections(CustomPlace::Before(CustomPlaceAnchor::Code));

        if !list.is_empty() {
            let mut branch_hints = wasm_encoder::BranchHints::new();
            let mut code_section = wasm_encoder::CodeSection::new();

            for func in list.iter() {
                let hints = func.encode(&mut code_section, dwarf.as_deref_mut());
                if !hints.is_empty() {
                    branch_hints.function_hints(func_index, hints.into_iter());
                }
                func_index += 1;
            }

            // Branch hints section has to be inserted before the Code section
            // Insert the section only if we have some hints
            if !branch_hints.is_empty() {
                self.wasm.section(&branch_hints);
            }

            // Finally, insert the Code section from the tmp buffer
            self.wasm.section(&code_section);

            if let Some(dwarf) = &mut dwarf {
                dwarf.set_code_section_size(code_section.byte_len());
            }
        }
        self.custom_sections(CustomPlace::After(CustomPlaceAnchor::Code));
    }
}

trait SectionItem {
    type Section: wasm_encoder::Section + Default;
    const ANCHOR: CustomPlaceAnchor;

    fn encode(&self, section: &mut Self::Section);
}

impl<T> SectionItem for &T
where
    T: SectionItem,
{
    type Section = T::Section;
    const ANCHOR: CustomPlaceAnchor = T::ANCHOR;

    fn encode(&self, section: &mut Self::Section) {
        T::encode(self, section)
    }
}

impl From<&FunctionType<'_>> for wasm_encoder::FuncType {
    fn from(ft: &FunctionType) -> Self {
        wasm_encoder::FuncType::new(
            ft.params.iter().map(|(_, _, ty)| (*ty).into()),
            ft.results.iter().map(|ty| (*ty).into()),
        )
    }
}

impl From<&StructType<'_>> for wasm_encoder::StructType {
    fn from(st: &StructType) -> wasm_encoder::StructType {
        wasm_encoder::StructType {
            fields: st.fields.iter().map(|f| f.into()).collect(),
        }
    }
}

impl From<&StructField<'_>> for wasm_encoder::FieldType {
    fn from(f: &StructField) -> wasm_encoder::FieldType {
        wasm_encoder::FieldType {
            element_type: f.ty.into(),
            mutable: f.mutable,
        }
    }
}

impl From<&ArrayType<'_>> for wasm_encoder::ArrayType {
    fn from(at: &ArrayType) -> Self {
        let field = wasm_encoder::FieldType {
            element_type: at.ty.into(),
            mutable: at.mutable,
        };
        wasm_encoder::ArrayType(field)
    }
}

impl From<&ContType<'_>> for wasm_encoder::ContType {
    fn from(at: &ContType) -> Self {
        wasm_encoder::ContType(at.0.into())
    }
}

enum RecOrType<'a> {
    Type(&'a Type<'a>),
    Rec(&'a Rec<'a>),
}

impl SectionItem for RecOrType<'_> {
    type Section = wasm_encoder::TypeSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Type;

    fn encode(&self, types: &mut wasm_encoder::TypeSection) {
        match self {
            RecOrType::Type(ty) => types.ty().subtype(&ty.to_subtype()),
            RecOrType::Rec(rec) => types.ty().rec(rec.types.iter().map(|t| t.to_subtype())),
        }
    }
}

impl Type<'_> {
    pub(crate) fn to_subtype(&self) -> wasm_encoder::SubType {
        self.def.to_subtype()
    }
}

impl TypeDef<'_> {
    pub(crate) fn to_subtype(&self) -> wasm_encoder::SubType {
        use wasm_encoder::CompositeInnerType::*;
        let composite_type = wasm_encoder::CompositeType {
            inner: match &self.kind {
                InnerTypeKind::Func(ft) => Func(ft.into()),
                InnerTypeKind::Struct(st) => Struct(st.into()),
                InnerTypeKind::Array(at) => Array(at.into()),
                InnerTypeKind::Cont(ct) => Cont(ct.into()),
            },
            shared: self.shared,
        };
        wasm_encoder::SubType {
            composite_type,
            is_final: self.final_type.unwrap_or(true),
            supertype_idx: self.parent.map(|i| i.unwrap_u32()),
        }
    }
}

impl From<ValType<'_>> for wasm_encoder::ValType {
    fn from(ty: ValType) -> Self {
        match ty {
            ValType::I32 => Self::I32,
            ValType::I64 => Self::I64,
            ValType::F32 => Self::F32,
            ValType::F64 => Self::F64,
            ValType::V128 => Self::V128,
            ValType::Ref(r) => Self::Ref(r.into()),
        }
    }
}

impl From<RefType<'_>> for wasm_encoder::RefType {
    fn from(r: RefType<'_>) -> Self {
        wasm_encoder::RefType {
            nullable: r.nullable,
            heap_type: r.heap.into(),
        }
    }
}

impl From<HeapType<'_>> for wasm_encoder::HeapType {
    fn from(r: HeapType<'_>) -> Self {
        use wasm_encoder::AbstractHeapType::*;
        match r {
            HeapType::Abstract { shared, ty } => {
                let ty = match ty {
                    AbstractHeapType::Func => Func,
                    AbstractHeapType::Extern => Extern,
                    AbstractHeapType::Exn => Exn,
                    AbstractHeapType::NoExn => NoExn,
                    AbstractHeapType::Any => Any,
                    AbstractHeapType::Eq => Eq,
                    AbstractHeapType::Struct => Struct,
                    AbstractHeapType::Array => Array,
                    AbstractHeapType::NoFunc => NoFunc,
                    AbstractHeapType::NoExtern => NoExtern,
                    AbstractHeapType::None => None,
                    AbstractHeapType::I31 => I31,
                    AbstractHeapType::Cont => Cont,
                    AbstractHeapType::NoCont => NoCont,
                };
                Self::Abstract { shared, ty }
            }
            HeapType::Concrete(i) => Self::Concrete(i.unwrap_u32()),
        }
    }
}

impl Encode for Option<Id<'_>> {
    fn encode(&self, _e: &mut Vec<u8>) {
        // used for parameters in the tuple impl as well as instruction labels
    }
}

impl<'a> Encode for ValType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        wasm_encoder::Encode::encode(&wasm_encoder::ValType::from(*self), e)
    }
}

impl<'a> Encode for HeapType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        wasm_encoder::Encode::encode(&wasm_encoder::HeapType::from(*self), e)
    }
}

impl From<StorageType<'_>> for wasm_encoder::StorageType {
    fn from(st: StorageType) -> Self {
        use wasm_encoder::StorageType::*;
        match st {
            StorageType::I8 => I8,
            StorageType::I16 => I16,
            StorageType::Val(vt) => Val(vt.into()),
        }
    }
}

impl SectionItem for Import<'_> {
    type Section = wasm_encoder::ImportSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Import;

    fn encode(&self, section: &mut wasm_encoder::ImportSection) {
        section.import(self.module, self.field, self.item.to_entity_type());
    }
}

impl ItemSig<'_> {
    pub(crate) fn to_entity_type(&self) -> wasm_encoder::EntityType {
        self.kind.to_entity_type()
    }
}

impl ItemKind<'_> {
    fn to_entity_type(&self) -> wasm_encoder::EntityType {
        use wasm_encoder::EntityType as ET;
        match self {
            ItemKind::Func(t) => ET::Function(t.unwrap_u32()),
            ItemKind::Table(t) => ET::Table(t.to_table_type()),
            ItemKind::Memory(t) => ET::Memory(t.to_memory_type()),
            ItemKind::Global(t) => ET::Global(t.to_global_type()),
            ItemKind::Tag(t) => ET::Tag(t.to_tag_type()),
        }
    }
}

impl TableType<'_> {
    fn to_table_type(&self) -> wasm_encoder::TableType {
        wasm_encoder::TableType {
            element_type: self.elem.into(),
            minimum: self.limits.min,
            maximum: self.limits.max,
            table64: self.limits.is64,
            shared: self.shared,
        }
    }
}

impl MemoryType {
    fn to_memory_type(&self) -> wasm_encoder::MemoryType {
        wasm_encoder::MemoryType {
            minimum: self.limits.min,
            maximum: self.limits.max,
            memory64: self.limits.is64,
            shared: self.shared,
            page_size_log2: self.page_size_log2,
        }
    }
}

impl GlobalType<'_> {
    fn to_global_type(&self) -> wasm_encoder::GlobalType {
        wasm_encoder::GlobalType {
            val_type: self.ty.into(),
            mutable: self.mutable,
            shared: self.shared,
        }
    }
}

impl TagType<'_> {
    fn to_tag_type(&self) -> wasm_encoder::TagType {
        match self {
            TagType::Exception(r) => wasm_encoder::TagType {
                kind: wasm_encoder::TagKind::Exception,
                func_type_idx: r.unwrap_u32(),
            },
        }
    }
}

impl<T> TypeUse<'_, T> {
    fn unwrap_u32(&self) -> u32 {
        self.index
            .as_ref()
            .expect("TypeUse should be filled in by this point")
            .unwrap_u32()
    }
}

struct FuncSectionTy<'a>(&'a TypeUse<'a, FunctionType<'a>>);

impl SectionItem for FuncSectionTy<'_> {
    type Section = wasm_encoder::FunctionSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Func;

    fn encode(&self, section: &mut wasm_encoder::FunctionSection) {
        section.function(self.0.unwrap_u32());
    }
}

impl Encode for Index<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.unwrap_u32().encode(e)
    }
}

impl Index<'_> {
    fn unwrap_u32(&self) -> u32 {
        match self {
            Index::Num(n, _) => *n,
            Index::Id(n) => panic!("unresolved index in emission: {:?}", n),
        }
    }
}

impl From<Index<'_>> for u32 {
    fn from(i: Index<'_>) -> Self {
        match i {
            Index::Num(i, _) => i,
            Index::Id(_) => unreachable!("unresolved index in encoding: {:?}", i),
        }
    }
}

impl SectionItem for Table<'_> {
    type Section = wasm_encoder::TableSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Table;

    fn encode(&self, section: &mut wasm_encoder::TableSection) {
        assert!(self.exports.names.is_empty());
        match &self.kind {
            TableKind::Normal {
                ty,
                init_expr: None,
            } => {
                section.table(ty.to_table_type());
            }
            TableKind::Normal {
                ty,
                init_expr: Some(init_expr),
            } => {
                section.table_with_init(ty.to_table_type(), &init_expr.to_const_expr());
            }
            _ => panic!("TableKind should be normal during encoding"),
        }
    }
}

impl SectionItem for Memory<'_> {
    type Section = wasm_encoder::MemorySection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Memory;

    fn encode(&self, section: &mut wasm_encoder::MemorySection) {
        assert!(self.exports.names.is_empty());
        match &self.kind {
            MemoryKind::Normal(t) => {
                section.memory(t.to_memory_type());
            }
            _ => panic!("MemoryKind should be normal during encoding"),
        }
    }
}

impl SectionItem for Global<'_> {
    type Section = wasm_encoder::GlobalSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Global;

    fn encode(&self, section: &mut wasm_encoder::GlobalSection) {
        assert!(self.exports.names.is_empty());
        let init = match &self.kind {
            GlobalKind::Inline(expr) => expr.to_const_expr(),
            _ => panic!("GlobalKind should be inline during encoding"),
        };
        section.global(self.ty.to_global_type(), &init);
    }
}

impl SectionItem for Export<'_> {
    type Section = wasm_encoder::ExportSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Export;

    fn encode(&self, section: &mut wasm_encoder::ExportSection) {
        section.export(self.name, self.kind.into(), self.item.unwrap_u32());
    }
}

impl From<ExportKind> for wasm_encoder::ExportKind {
    fn from(kind: ExportKind) -> Self {
        match kind {
            ExportKind::Func => Self::Func,
            ExportKind::Table => Self::Table,
            ExportKind::Memory => Self::Memory,
            ExportKind::Global => Self::Global,
            ExportKind::Tag => Self::Tag,
        }
    }
}

impl SectionItem for Elem<'_> {
    type Section = wasm_encoder::ElementSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Elem;

    fn encode(&self, section: &mut wasm_encoder::ElementSection) {
        use wasm_encoder::Elements;

        let elements = match &self.payload {
            ElemPayload::Indices(v) => {
                Elements::Functions(Cow::Owned(v.iter().map(|i| i.unwrap_u32()).collect()))
            }
            ElemPayload::Exprs { exprs, ty } => Elements::Expressions(
                (*ty).into(),
                Cow::Owned(exprs.iter().map(|e| e.to_const_expr()).collect()),
            ),
        };
        match &self.kind {
            ElemKind::Active { table, offset } => {
                section.active(
                    table.map(|t| t.unwrap_u32()),
                    &offset.to_const_expr(),
                    elements,
                );
            }
            ElemKind::Passive => {
                section.passive(elements);
            }
            ElemKind::Declared => {
                section.declared(elements);
            }
        }
    }
}

impl SectionItem for Data<'_> {
    type Section = wasm_encoder::DataSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Data;

    fn encode(&self, section: &mut wasm_encoder::DataSection) {
        let mut data = Vec::new();
        for val in self.data.iter() {
            val.push_onto(&mut data);
        }
        match &self.kind {
            DataKind::Passive => {
                section.passive(data);
            }
            DataKind::Active { memory, offset } => {
                section.active(memory.unwrap_u32(), &offset.to_const_expr(), data);
            }
        }
    }
}

impl Func<'_> {
    /// Encodes the function into `e` while returning all branch hints with
    /// known relative offsets after encoding.
    ///
    /// The `dwarf` field is optional and used to track debugging information
    /// for each instruction.
    fn encode(
        &self,
        section: &mut wasm_encoder::CodeSection,
        mut dwarf: Option<&mut dwarf::Dwarf>,
    ) -> Vec<wasm_encoder::BranchHint> {
        assert!(self.exports.names.is_empty());
        let (expr, locals) = match &self.kind {
            FuncKind::Inline { expression, locals } => (expression, locals),
            _ => panic!("should only have inline functions in emission"),
        };

        if let Some(dwarf) = &mut dwarf {
            let index = match self.ty.index.as_ref().unwrap() {
                Index::Num(n, _) => *n,
                _ => unreachable!(),
            };
            dwarf.start_func(self.span, index, locals);
        }

        // Encode the function into a temporary vector because functions are
        // prefixed with their length. The temporary vector, when encoded,
        // encodes its length first then the body.
        let mut func =
            wasm_encoder::Function::new_with_locals_types(locals.iter().map(|t| t.ty.into()));
        let branch_hints = expr.encode(&mut func, dwarf.as_deref_mut());
        let func_size = func.byte_len();
        section.function(&func);

        if let Some(dwarf) = &mut dwarf {
            dwarf.end_func(func_size, section.byte_len());
        }

        branch_hints
    }
}

impl Expression<'_> {
    /// Encodes this expression into `e` and optionally tracks debugging
    /// information for each instruction in `dwarf`.
    ///
    /// Returns all branch hints, if any, found while parsing this function.
    fn encode(
        &self,
        func: &mut wasm_encoder::Function,
        mut dwarf: Option<&mut dwarf::Dwarf>,
    ) -> Vec<wasm_encoder::BranchHint> {
        let mut hints = Vec::with_capacity(self.branch_hints.len());
        let mut next_hint = self.branch_hints.iter().peekable();
        let mut tmp = Vec::new();

        for (i, instr) in self.instrs.iter().enumerate() {
            // Branch hints are stored in order of increasing `instr_index` so
            // check to see if the next branch hint matches this instruction's
            // index.
            if let Some(hint) = next_hint.next_if(|h| h.instr_index == i) {
                hints.push(wasm_encoder::BranchHint {
                    branch_func_offset: u32::try_from(func.byte_len() + tmp.len()).unwrap(),
                    branch_hint_value: hint.value,
                });
            }

            // If DWARF is enabled then track this instruction's binary offset
            // and source location.
            if let Some(dwarf) = &mut dwarf {
                if let Some(span) = self.instr_spans.as_ref().map(|s| s[i]) {
                    dwarf.instr(func.byte_len() + tmp.len(), span);
                }
            }

            // Finally emit the instruction and move to the next.
            instr.encode(&mut tmp);
        }
        func.raw(tmp.iter().copied());
        func.instruction(&wasm_encoder::Instruction::End);

        hints
    }

    fn to_const_expr(&self) -> wasm_encoder::ConstExpr {
        let mut tmp = Vec::new();
        for instr in self.instrs.iter() {
            instr.encode(&mut tmp);
        }
        wasm_encoder::ConstExpr::raw(tmp)
    }
}

impl Encode for BlockType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        // block types using an index are encoded as an sleb, not a uleb
        if let Some(Index::Num(n, _)) = &self.ty.index {
            return i64::from(*n).encode(e);
        }
        let ty = self
            .ty
            .inline
            .as_ref()
            .expect("function type not filled in");
        if ty.params.is_empty() && ty.results.is_empty() {
            return e.push(0x40);
        }
        if ty.params.is_empty() && ty.results.len() == 1 {
            return ty.results[0].encode(e);
        }
        panic!("multi-value block types should have an index");
    }
}

impl Encode for LaneArg {
    fn encode(&self, e: &mut Vec<u8>) {
        self.lane.encode(e);
    }
}

impl Encode for MemArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.memory {
            Index::Num(0, _) => {
                self.align.trailing_zeros().encode(e);
                self.offset.encode(e);
            }
            _ => {
                (self.align.trailing_zeros() | (1 << 6)).encode(e);
                self.memory.encode(e);
                self.offset.encode(e);
            }
        }
    }
}

impl Encode for Ordering {
    fn encode(&self, buf: &mut Vec<u8>) {
        let flag: u8 = match self {
            Ordering::SeqCst => 0,
            Ordering::AcqRel => 1,
        };
        flag.encode(buf);
    }
}

impl<T> Encode for Ordered<T>
where
    T: Encode,
{
    fn encode(&self, buf: &mut Vec<u8>) {
        self.ordering.encode(buf);
        self.inner.encode(buf);
    }
}

impl Encode for LoadOrStoreLane<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.memarg.encode(e);
        self.lane.encode(e);
    }
}

impl Encode for CallIndirect<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.unwrap_u32().encode(e);
        self.table.encode(e);
    }
}

impl Encode for TableInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.elem.encode(e);
        self.table.encode(e);
    }
}

impl Encode for TableCopy<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
        self.src.encode(e);
    }
}

impl Encode for TableArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
    }
}

impl Encode for MemoryArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.mem.encode(e);
    }
}

impl Encode for MemoryInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.data.encode(e);
        self.mem.encode(e);
    }
}

impl Encode for MemoryCopy<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
        self.src.encode(e);
    }
}

impl Encode for BrTableIndices<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.labels.encode(e);
        self.default.encode(e);
    }
}

impl Encode for F32 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.extend_from_slice(&self.bits.to_le_bytes());
    }
}

impl Encode for F64 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.extend_from_slice(&self.bits.to_le_bytes());
    }
}

#[derive(Default)]
struct Names<'a> {
    module: Option<&'a str>,
    funcs: Vec<(u32, &'a str)>,
    func_idx: u32,
    locals: Vec<(u32, Vec<(u32, &'a str)>)>,
    labels: Vec<(u32, Vec<(u32, &'a str)>)>,
    globals: Vec<(u32, &'a str)>,
    global_idx: u32,
    memories: Vec<(u32, &'a str)>,
    memory_idx: u32,
    tables: Vec<(u32, &'a str)>,
    table_idx: u32,
    tags: Vec<(u32, &'a str)>,
    tag_idx: u32,
    types: Vec<(u32, &'a str)>,
    type_idx: u32,
    data: Vec<(u32, &'a str)>,
    data_idx: u32,
    elems: Vec<(u32, &'a str)>,
    elem_idx: u32,
    fields: Vec<(u32, Vec<(u32, &'a str)>)>,
}

fn find_names<'a>(
    module_id: &Option<Id<'a>>,
    module_name: &Option<NameAnnotation<'a>>,
    fields: &[ModuleField<'a>],
) -> Names<'a> {
    fn get_name<'a>(id: &Option<Id<'a>>, name: &Option<NameAnnotation<'a>>) -> Option<&'a str> {
        name.as_ref().map(|n| n.name).or(id.and_then(|id| {
            if id.is_gensym() {
                None
            } else {
                Some(id.name())
            }
        }))
    }

    enum Name {
        Type,
        Global,
        Func,
        Memory,
        Table,
        Tag,
        Elem,
        Data,
    }

    let mut ret = Names::default();
    ret.module = get_name(module_id, module_name);
    let mut names = Vec::new();
    for field in fields {
        // Extract the kind/id/name from whatever kind of field this is...
        let (kind, id, name) = match field {
            ModuleField::Import(i) => (
                match i.item.kind {
                    ItemKind::Func(_) => Name::Func,
                    ItemKind::Table(_) => Name::Table,
                    ItemKind::Memory(_) => Name::Memory,
                    ItemKind::Global(_) => Name::Global,
                    ItemKind::Tag(_) => Name::Tag,
                },
                &i.item.id,
                &i.item.name,
            ),
            ModuleField::Global(g) => (Name::Global, &g.id, &g.name),
            ModuleField::Table(t) => (Name::Table, &t.id, &t.name),
            ModuleField::Memory(m) => (Name::Memory, &m.id, &m.name),
            ModuleField::Tag(t) => (Name::Tag, &t.id, &t.name),
            ModuleField::Type(t) => (Name::Type, &t.id, &t.name),
            ModuleField::Rec(r) => {
                for ty in &r.types {
                    names.push((Name::Type, &ty.id, &ty.name, field));
                }
                continue;
            }
            ModuleField::Elem(e) => (Name::Elem, &e.id, &e.name),
            ModuleField::Data(d) => (Name::Data, &d.id, &d.name),
            ModuleField::Func(f) => (Name::Func, &f.id, &f.name),
            ModuleField::Export(_) | ModuleField::Start(_) | ModuleField::Custom(_) => continue,
        };
        names.push((kind, id, name, field));
    }

    for (kind, id, name, field) in names {
        // .. and using the kind we can figure out where to place this name
        let (list, idx) = match kind {
            Name::Func => (&mut ret.funcs, &mut ret.func_idx),
            Name::Table => (&mut ret.tables, &mut ret.table_idx),
            Name::Memory => (&mut ret.memories, &mut ret.memory_idx),
            Name::Global => (&mut ret.globals, &mut ret.global_idx),
            Name::Tag => (&mut ret.tags, &mut ret.tag_idx),
            Name::Type => (&mut ret.types, &mut ret.type_idx),
            Name::Elem => (&mut ret.elems, &mut ret.elem_idx),
            Name::Data => (&mut ret.data, &mut ret.data_idx),
        };
        if let Some(name) = get_name(id, name) {
            list.push((*idx, name));
        }

        // Handle module locals separately from above
        if let ModuleField::Func(f) = field {
            let mut local_names = Vec::new();
            let mut label_names = Vec::new();
            let mut local_idx = 0;
            let mut label_idx = 0;

            // Consult the inline type listed for local names of parameters.
            // This is specifically preserved during the name resolution
            // pass, but only for functions, so here we can look at the
            // original source's names.
            if let Some(ty) = &f.ty.inline {
                for (id, name, _) in ty.params.iter() {
                    if let Some(name) = get_name(id, name) {
                        local_names.push((local_idx, name));
                    }
                    local_idx += 1;
                }
            }
            if let FuncKind::Inline {
                locals, expression, ..
            } = &f.kind
            {
                for local in locals.iter() {
                    if let Some(name) = get_name(&local.id, &local.name) {
                        local_names.push((local_idx, name));
                    }
                    local_idx += 1;
                }

                for i in expression.instrs.iter() {
                    match i {
                        Instruction::If(block)
                        | Instruction::Block(block)
                        | Instruction::Loop(block)
                        | Instruction::Try(block)
                        | Instruction::TryTable(TryTable { block, .. }) => {
                            if let Some(name) = get_name(&block.label, &block.label_name) {
                                label_names.push((label_idx, name));
                            }
                            label_idx += 1;
                        }
                        _ => {}
                    }
                }
            }
            if local_names.len() > 0 {
                ret.locals.push((*idx, local_names));
            }
            if label_names.len() > 0 {
                ret.labels.push((*idx, label_names));
            }
        }

        // Handle struct fields separately from above
        if let ModuleField::Type(ty) = field {
            let mut field_names = vec![];
            match &ty.def.kind {
                InnerTypeKind::Func(_) | InnerTypeKind::Array(_) | InnerTypeKind::Cont(_) => {}
                InnerTypeKind::Struct(ty_struct) => {
                    for (idx, field) in ty_struct.fields.iter().enumerate() {
                        if let Some(name) = get_name(&field.id, &None) {
                            field_names.push((idx as u32, name))
                        }
                    }
                }
            }
            if field_names.len() > 0 {
                ret.fields.push((*idx, field_names))
            }
        }

        *idx += 1;
    }

    return ret;
}

impl Names<'_> {
    fn is_empty(&self) -> bool {
        self.module.is_none()
            && self.funcs.is_empty()
            && self.locals.is_empty()
            && self.labels.is_empty()
            && self.globals.is_empty()
            && self.memories.is_empty()
            && self.tables.is_empty()
            && self.types.is_empty()
            && self.elems.is_empty()
            && self.data.is_empty()
            && self.fields.is_empty()
            && self.tags.is_empty()
    }
}

impl Names<'_> {
    fn to_name_section(&self) -> wasm_encoder::NameSection {
        let mut names = wasm_encoder::NameSection::default();

        if let Some(id) = self.module {
            names.module(id);
        }
        let name_map = |indices: &[(u32, &str)]| {
            if indices.is_empty() {
                return None;
            }
            let mut map = wasm_encoder::NameMap::default();
            for (idx, name) in indices {
                map.append(*idx, *name);
            }
            Some(map)
        };
        let indirect_name_map = |indices: &[(u32, Vec<(u32, &str)>)]| {
            if indices.is_empty() {
                return None;
            }
            let mut map = wasm_encoder::IndirectNameMap::default();
            for (idx, names) in indices {
                if let Some(names) = name_map(names) {
                    map.append(*idx, &names);
                }
            }
            Some(map)
        };
        if let Some(map) = name_map(&self.funcs) {
            names.functions(&map);
        }
        if let Some(map) = indirect_name_map(&self.locals) {
            names.locals(&map);
        }
        if let Some(map) = indirect_name_map(&self.labels) {
            names.labels(&map);
        }
        if let Some(map) = name_map(&self.types) {
            names.types(&map);
        }
        if let Some(map) = name_map(&self.tables) {
            names.tables(&map);
        }
        if let Some(map) = name_map(&self.memories) {
            names.memories(&map);
        }
        if let Some(map) = name_map(&self.globals) {
            names.globals(&map);
        }
        if let Some(map) = name_map(&self.elems) {
            names.elements(&map);
        }
        if let Some(map) = name_map(&self.data) {
            names.data(&map);
        }
        if let Some(map) = indirect_name_map(&self.fields) {
            names.fields(&map);
        }
        if let Some(map) = name_map(&self.tags) {
            names.tags(&map);
        }
        names
    }
}

impl Encode for Id<'_> {
    fn encode(&self, dst: &mut Vec<u8>) {
        assert!(!self.is_gensym());
        self.name().encode(dst);
    }
}

impl<'a> Encode for TryTable<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.block.encode(dst);
        self.catches.encode(dst);
    }
}

impl<'a> Encode for TryTableCatch<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        let flag_byte: u8 = match self.kind {
            TryTableCatchKind::Catch(..) => 0,
            TryTableCatchKind::CatchRef(..) => 1,
            TryTableCatchKind::CatchAll => 2,
            TryTableCatchKind::CatchAllRef => 3,
        };
        flag_byte.encode(dst);
        match self.kind {
            TryTableCatchKind::Catch(tag) | TryTableCatchKind::CatchRef(tag) => {
                tag.encode(dst);
            }
            TryTableCatchKind::CatchAll | TryTableCatchKind::CatchAllRef => {}
        }
        self.label.encode(dst);
    }
}

impl<'a> Encode for ContBind<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.argument_index.encode(dst);
        self.result_index.encode(dst);
    }
}

impl<'a> Encode for Resume<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.type_index.encode(dst);
        self.table.encode(dst);
    }
}

impl<'a> Encode for ResumeThrow<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.type_index.encode(dst);
        self.tag_index.encode(dst);
        self.table.encode(dst);
    }
}

impl<'a> Encode for ResumeTable<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.handlers.encode(dst);
    }
}

impl<'a> Encode for Handle<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        match self {
            Handle::OnLabel { tag, label } => {
                dst.push(0x00);
                tag.encode(dst);
                label.encode(dst);
            }
            Handle::OnSwitch { tag } => {
                dst.push(0x01);
                tag.encode(dst);
            }
        }
    }
}

impl<'a> Encode for Switch<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.type_index.encode(dst);
        self.tag_index.encode(dst);
    }
}

impl Encode for V128Const {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.to_le_bytes());
    }
}

impl Encode for I8x16Shuffle {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.lanes);
    }
}

impl<'a> Encode for SelectTypes<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        match &self.tys {
            Some(list) => {
                dst.push(0x1c);
                list.encode(dst);
            }
            None => dst.push(0x1b),
        }
    }
}

impl Custom<'_> {
    fn encode(&self, module: &mut wasm_encoder::Module) {
        match self {
            Custom::Raw(r) => {
                module.section(&r.to_section());
            }
            Custom::Producers(p) => {
                module.section(&p.to_section());
            }
            Custom::Dylink0(p) => {
                module.section(&p.to_section());
            }
        }
    }
}

impl RawCustomSection<'_> {
    fn to_section(&self) -> wasm_encoder::CustomSection<'_> {
        let mut ret = Vec::new();
        for list in self.data.iter() {
            ret.extend_from_slice(list);
        }
        wasm_encoder::CustomSection {
            name: self.name.into(),
            data: ret.into(),
        }
    }
}

impl Producers<'_> {
    pub(crate) fn to_section(&self) -> wasm_encoder::ProducersSection {
        let mut ret = wasm_encoder::ProducersSection::default();
        for (name, fields) in self.fields.iter() {
            let mut field = wasm_encoder::ProducersField::new();
            for (key, value) in fields {
                field.value(key, value);
            }
            ret.field(name, &field);
        }
        ret
    }
}

impl Dylink0<'_> {
    fn to_section(&self) -> wasm_encoder::CustomSection<'_> {
        let mut e = Vec::new();
        for section in self.subsections.iter() {
            e.push(section.id());
            let mut tmp = Vec::new();
            section.encode(&mut tmp);
            tmp.encode(&mut e);
        }
        wasm_encoder::CustomSection {
            name: "dylink.0".into(),
            data: e.into(),
        }
    }
}

impl Encode for Dylink0Subsection<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            Dylink0Subsection::MemInfo {
                memory_size,
                memory_align,
                table_size,
                table_align,
            } => {
                memory_size.encode(e);
                memory_align.encode(e);
                table_size.encode(e);
                table_align.encode(e);
            }
            Dylink0Subsection::Needed(libs) => libs.encode(e),
            Dylink0Subsection::ExportInfo(list) => list.encode(e),
            Dylink0Subsection::ImportInfo(list) => list.encode(e),
        }
    }
}

impl SectionItem for Tag<'_> {
    type Section = wasm_encoder::TagSection;
    const ANCHOR: CustomPlaceAnchor = CustomPlaceAnchor::Tag;

    fn encode(&self, section: &mut wasm_encoder::TagSection) {
        section.tag(self.ty.to_tag_type());
        match &self.kind {
            TagKind::Inline() => {}
            _ => panic!("TagKind should be inline during encoding"),
        }
    }
}

impl Encode for StructAccess<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.r#struct.encode(e);
        self.field.encode(e);
    }
}

impl Encode for ArrayFill<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.array.encode(e);
    }
}

impl Encode for ArrayCopy<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dest_array.encode(e);
        self.src_array.encode(e);
    }
}

impl Encode for ArrayInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.array.encode(e);
        self.segment.encode(e);
    }
}

impl Encode for ArrayNewFixed<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.array.encode(e);
        self.length.encode(e);
    }
}

impl Encode for ArrayNewData<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.array.encode(e);
        self.data_idx.encode(e);
    }
}

impl Encode for ArrayNewElem<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.array.encode(e);
        self.elem_idx.encode(e);
    }
}

impl Encode for RefTest<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0xfb);
        if self.r#type.nullable {
            e.push(0x15);
        } else {
            e.push(0x14);
        }
        self.r#type.heap.encode(e);
    }
}

impl Encode for RefCast<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0xfb);
        if self.r#type.nullable {
            e.push(0x17);
        } else {
            e.push(0x16);
        }
        self.r#type.heap.encode(e);
    }
}

fn br_on_cast_flags(from_nullable: bool, to_nullable: bool) -> u8 {
    let mut flag = 0;
    if from_nullable {
        flag |= 1 << 0;
    }
    if to_nullable {
        flag |= 1 << 1;
    }
    flag
}

impl Encode for BrOnCast<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0xfb);
        e.push(0x18);
        e.push(br_on_cast_flags(
            self.from_type.nullable,
            self.to_type.nullable,
        ));
        self.label.encode(e);
        self.from_type.heap.encode(e);
        self.to_type.heap.encode(e);
    }
}

impl Encode for BrOnCastFail<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0xfb);
        e.push(0x19);
        e.push(br_on_cast_flags(
            self.from_type.nullable,
            self.to_type.nullable,
        ));
        self.label.encode(e);
        self.from_type.heap.encode(e);
        self.to_type.heap.encode(e);
    }
}
