//! Dummy version of dwarf emission that does nothing when the compile-time
//! feature is disabled.

use crate::core::binary::{EncodeOptions, Encoder, Names, RecOrType};
use crate::core::Local;
use crate::token::Span;

pub struct Dwarf<'a> {
    uninhabited: &'a std::convert::Infallible,
}

impl<'a> Dwarf<'a> {
    pub fn new(
        _func_imports: u32,
        _opts: &EncodeOptions<'a>,
        _names: &Names<'a>,
        _types: &'a [RecOrType<'a>],
    ) -> Option<Dwarf<'a>> {
        None
    }

    pub fn start_func(&mut self, _span: Span, _ty: u32, _locals: &[Local<'_>]) {
        match *self.uninhabited {}
    }

    pub fn instr(&mut self, _offset: usize, _span: Span) {
        match *self.uninhabited {}
    }

    pub fn end_func(&mut self, _: usize, _: usize) {
        match *self.uninhabited {}
    }

    pub fn set_code_section_size(&mut self, _size: usize) {
        match *self.uninhabited {}
    }

    pub fn emit(&mut self, _dst: &mut Encoder<'_>) {
        match *self.uninhabited {}
    }
}
