use crate::client::MlsError;
use alloc::vec::Vec;
use mls_rs_codec::{MlsEncode, MlsSize};

pub type ComponentID = u32;

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode)]
pub struct ComponentOperationLabel<'a> {
    label: &'static [u8],
    component_id: ComponentID,
    context: &'a [u8],
}

impl<'a> ComponentOperationLabel<'a> {
    pub fn new(component_id: u32, context: &'a [u8]) -> Self {
        Self {
            label: b"MLS 1.0 Application",
            component_id,
            context,
        }
    }

    pub fn get_bytes(&self) -> Result<Vec<u8>, MlsError> {
        self.mls_encode_to_vec().map_err(Into::into)
    }
}
