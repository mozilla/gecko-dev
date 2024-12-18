// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::{Extension, ExtensionError, ExtensionType, MlsExtension};
use alloc::vec::Vec;
use core::ops::Deref;
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

/// A collection of MLS [Extensions](super::Extension).
///
///
/// # Warning
///
/// Extension lists require that each type of extension has at most one entry.
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Debug, Clone, Default, MlsSize, MlsEncode, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ExtensionList(Vec<Extension>);

impl Deref for ExtensionList {
    type Target = Vec<Extension>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl PartialEq for ExtensionList {
    fn eq(&self, other: &Self) -> bool {
        self.len() == other.len()
            && self
                .iter()
                .all(|ext| other.get(ext.extension_type).as_ref() == Some(ext))
    }
}

impl MlsDecode for ExtensionList {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, mls_rs_codec::Error> {
        mls_rs_codec::iter::mls_decode_collection(reader, |data| {
            let mut list = ExtensionList::new();

            while !data.is_empty() {
                let ext = Extension::mls_decode(data)?;
                let ext_type = ext.extension_type;

                if list.0.iter().any(|e| e.extension_type == ext_type) {
                    // #[cfg(feature = "std")]
                    // return Err(mls_rs_codec::Error::Custom(format!(
                    //    "Extension list has duplicate extension of type {ext_type:?}"
                    // )));

                    // #[cfg(not(feature = "std"))]
                    return Err(mls_rs_codec::Error::Custom(1));
                }

                list.0.push(ext);
            }

            Ok(list)
        })
    }
}

impl From<Vec<Extension>> for ExtensionList {
    fn from(extensions: Vec<Extension>) -> Self {
        extensions.into_iter().collect()
    }
}

impl Extend<Extension> for ExtensionList {
    fn extend<T: IntoIterator<Item = Extension>>(&mut self, iter: T) {
        iter.into_iter().for_each(|ext| self.set(ext));
    }
}

impl FromIterator<Extension> for ExtensionList {
    fn from_iter<T: IntoIterator<Item = Extension>>(iter: T) -> Self {
        let mut list = Self::new();
        list.extend(iter);
        list
    }
}

impl ExtensionList {
    /// Create a new empty extension list.
    pub fn new() -> ExtensionList {
        Default::default()
    }

    /// Retrieve an extension by providing a type that implements the
    /// [MlsExtension](super::MlsExtension) trait.
    ///
    /// Returns an error if the underlying deserialization of the extension
    /// data fails.
    pub fn get_as<E: MlsExtension>(&self) -> Result<Option<E>, ExtensionError> {
        self.0
            .iter()
            .find(|e| e.extension_type == E::extension_type())
            .map(E::from_extension)
            .transpose()
    }

    /// Determine if a specific extension exists within the list.
    pub fn has_extension(&self, ext_id: ExtensionType) -> bool {
        self.0.iter().any(|e| e.extension_type == ext_id)
    }

    /// Set an extension in the list based on a provided type that implements
    /// the [MlsExtension](super::MlsExtension) trait.
    ///
    /// If there is already an entry in the list for the same extension type,
    /// then the prior value is removed as part of the insertion.
    ///
    /// This function will return an error if `ext` fails to serialize
    /// properly.
    pub fn set_from<E: MlsExtension>(&mut self, ext: E) -> Result<(), ExtensionError> {
        let ext = ext.into_extension()?;
        self.set(ext);
        Ok(())
    }

    /// Set an extension in the list based on a raw
    /// [Extension](super::Extension) value.
    ///
    /// If there is already an entry in the list for the same extension type,
    /// then the prior value is removed as part of the insertion.
    pub fn set(&mut self, ext: Extension) {
        let mut found = self
            .0
            .iter_mut()
            .find(|e| e.extension_type == ext.extension_type);

        if let Some(found) = found.take() {
            *found = ext;
        } else {
            self.0.push(ext);
        }
    }

    /// Get a raw [Extension](super::Extension) value based on an
    /// [ExtensionType](super::ExtensionType).
    pub fn get(&self, extension_type: ExtensionType) -> Option<Extension> {
        self.0
            .iter()
            .find(|e| e.extension_type == extension_type)
            .cloned()
    }

    /// Remove an extension from the list by
    /// [ExtensionType](super::ExtensionType)
    pub fn remove(&mut self, ext_type: ExtensionType) {
        self.0.retain(|e| e.extension_type != ext_type)
    }

    /// Append another extension list to this one.
    ///
    /// If there is already an entry in the list for the same extension type,
    /// then the existing value is removed.
    pub fn append(&mut self, others: Self) {
        self.0.extend(others.0);
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;
    use alloc::vec::Vec;
    use assert_matches::assert_matches;
    use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

    use crate::extension::{
        list::ExtensionList, Extension, ExtensionType, MlsCodecExtension, MlsExtension,
    };

    #[derive(Debug, Clone, MlsSize, MlsEncode, MlsDecode, PartialEq, Eq)]
    struct TestExtensionA(u32);

    #[derive(Debug, Clone, MlsEncode, MlsDecode, MlsSize, PartialEq, Eq)]
    struct TestExtensionB(#[mls_codec(with = "mls_rs_codec::byte_vec")] Vec<u8>);

    #[derive(Debug, Clone, MlsEncode, MlsDecode, MlsSize, PartialEq, Eq)]
    struct TestExtensionC(u8);

    impl MlsCodecExtension for TestExtensionA {
        fn extension_type() -> ExtensionType {
            ExtensionType(128)
        }
    }

    impl MlsCodecExtension for TestExtensionB {
        fn extension_type() -> ExtensionType {
            ExtensionType(129)
        }
    }

    impl MlsCodecExtension for TestExtensionC {
        fn extension_type() -> ExtensionType {
            ExtensionType(130)
        }
    }

    #[test]
    fn test_extension_list_get_set_from_get_as() {
        let mut list = ExtensionList::new();

        let ext_a = TestExtensionA(0);
        let ext_b = TestExtensionB(vec![1]);

        // Add the extensions to the list
        list.set_from(ext_a.clone()).unwrap();
        list.set_from(ext_b.clone()).unwrap();

        assert_eq!(list.len(), 2);
        assert_eq!(list.get_as::<TestExtensionA>().unwrap(), Some(ext_a));
        assert_eq!(list.get_as::<TestExtensionB>().unwrap(), Some(ext_b));
    }

    #[test]
    fn test_extension_list_get_set() {
        let mut list = ExtensionList::new();

        let ext_a = Extension::new(ExtensionType(254), vec![0, 1, 2]);
        let ext_b = Extension::new(ExtensionType(255), vec![4, 5, 6]);

        // Add the extensions to the list
        list.set(ext_a.clone());
        list.set(ext_b.clone());

        assert_eq!(list.len(), 2);
        assert_eq!(list.get(ExtensionType(254)), Some(ext_a));
        assert_eq!(list.get(ExtensionType(255)), Some(ext_b));
    }

    #[test]
    fn extension_list_can_overwrite_values() {
        let mut list = ExtensionList::new();

        let ext_1 = TestExtensionA(0);
        let ext_2 = TestExtensionA(1);

        list.set_from(ext_1).unwrap();
        list.set_from(ext_2.clone()).unwrap();

        assert_eq!(list.get_as::<TestExtensionA>().unwrap(), Some(ext_2));
    }

    #[test]
    fn extension_list_will_return_none_for_type_not_stored() {
        let mut list = ExtensionList::new();

        assert!(list.get_as::<TestExtensionA>().unwrap().is_none());

        assert!(list
            .get(<TestExtensionA as MlsCodecExtension>::extension_type())
            .is_none());

        list.set_from(TestExtensionA(1)).unwrap();

        assert!(list.get_as::<TestExtensionB>().unwrap().is_none());

        assert!(list
            .get(<TestExtensionB as MlsCodecExtension>::extension_type())
            .is_none());
    }

    #[test]
    fn test_extension_list_has_ext() {
        let mut list = ExtensionList::new();

        let ext = TestExtensionA(255);

        list.set_from(ext).unwrap();

        assert!(list.has_extension(<TestExtensionA as MlsCodecExtension>::extension_type()));
        assert!(!list.has_extension(42.into()));
    }

    #[derive(MlsEncode, MlsSize)]
    struct ExtensionsVec(Vec<Extension>);

    #[test]
    fn extension_list_is_serialized_like_a_sequence_of_extensions() {
        let extension_vec = vec![
            Extension::new(ExtensionType(128), vec![0, 1, 2, 3]),
            Extension::new(ExtensionType(129), vec![1, 2, 3, 4]),
        ];

        let extension_list: ExtensionList = ExtensionList::from(extension_vec.clone());

        assert_eq!(
            ExtensionsVec(extension_vec).mls_encode_to_vec().unwrap(),
            extension_list.mls_encode_to_vec().unwrap(),
        );
    }

    #[test]
    fn deserializing_extension_list_fails_on_duplicate_extension() {
        let extensions = ExtensionsVec(vec![
            TestExtensionA(1).into_extension().unwrap(),
            TestExtensionA(2).into_extension().unwrap(),
        ]);

        let serialized_extensions = extensions.mls_encode_to_vec().unwrap();

        assert_matches!(
            ExtensionList::mls_decode(&mut &*serialized_extensions),
            Err(mls_rs_codec::Error::Custom(_))
        );
    }

    #[test]
    fn extension_list_equality_does_not_consider_order() {
        let extensions = [
            TestExtensionA(33).into_extension().unwrap(),
            TestExtensionC(34).into_extension().unwrap(),
        ];

        let a = extensions.iter().cloned().collect::<ExtensionList>();
        let b = extensions.iter().rev().cloned().collect::<ExtensionList>();

        assert_eq!(a, b);
    }

    #[test]
    fn extending_extension_list_maintains_extension_uniqueness() {
        let mut list = ExtensionList::new();
        list.set_from(TestExtensionA(33)).unwrap();
        list.set_from(TestExtensionC(34)).unwrap();
        list.extend([
            TestExtensionA(35).into_extension().unwrap(),
            TestExtensionB(vec![36]).into_extension().unwrap(),
            TestExtensionA(37).into_extension().unwrap(),
        ]);

        let expected = ExtensionList(vec![
            TestExtensionA(37).into_extension().unwrap(),
            TestExtensionB(vec![36]).into_extension().unwrap(),
            TestExtensionC(34).into_extension().unwrap(),
        ]);

        assert_eq!(list, expected);
    }

    #[test]
    fn extension_list_from_vec_maintains_extension_uniqueness() {
        let list = ExtensionList::from(vec![
            TestExtensionA(33).into_extension().unwrap(),
            TestExtensionC(34).into_extension().unwrap(),
            TestExtensionA(35).into_extension().unwrap(),
        ]);

        let expected = ExtensionList(vec![
            TestExtensionA(35).into_extension().unwrap(),
            TestExtensionC(34).into_extension().unwrap(),
        ]);

        assert_eq!(list, expected);
    }
}
