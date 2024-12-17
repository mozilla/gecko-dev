// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use mls_rs_core::{error::IntoAnyError, identity::CertificateChain};

use crate::{
    DerCertificate, SubjectComponent, X509CertificateReader, X509IdentityError,
    X509IdentityExtractor,
};

#[derive(Debug, Clone)]
/// A utility to determine unique identity for use with MLS by reading
/// the subject of a certificate.
///
/// The default behavior of this struct is to try and produce an identity
/// based on the common name component of the subject. If a common name
/// component is not found, then the byte value of the entire subject
/// is used as a fallback.
pub struct SubjectIdentityExtractor<R: X509CertificateReader> {
    offset: usize,
    reader: R,
}

impl<R> SubjectIdentityExtractor<R>
where
    R: X509CertificateReader,
{
    /// Create a new identity extractor.
    ///
    /// `offset` is used to determine which certificate in a [`CertificateChain`]
    /// should be used to evaluate identity. A value of 0 indicates to use the
    /// leaf (first value) of the chain.
    pub fn new(offset: usize, reader: R) -> Self {
        Self { offset, reader }
    }

    fn extract_common_name(
        &self,
        certificate: &DerCertificate,
    ) -> Result<Option<SubjectComponent>, X509IdentityError> {
        Ok(self
            .reader
            .subject_components(certificate)
            .map_err(|err| X509IdentityError::IdentityExtractorError(err.into_any_error()))?
            .iter()
            .find(|component| matches!(component, SubjectComponent::CommonName(_)))
            .cloned())
    }

    /// Get a unique identifier for a `certificate_chain`.
    pub fn identity(
        &self,
        certificate_chain: &CertificateChain,
    ) -> Result<Vec<u8>, X509IdentityError> {
        let cert = get_certificate(certificate_chain, self.offset)?;

        let common_name_value = self.extract_common_name(cert)?;

        if let Some(SubjectComponent::CommonName(common_name)) = common_name_value {
            return Ok(common_name.as_bytes().to_vec());
        }

        self.subject_bytes(cert)
    }

    fn subject_bytes(&self, certificate: &DerCertificate) -> Result<Vec<u8>, X509IdentityError> {
        self.reader
            .subject_bytes(certificate)
            .map_err(|e| X509IdentityError::X509ReaderError(e.into_any_error()))
    }

    /// Determine if `successor` resolves to the same
    /// identity value as `predecessor`, indicating that
    /// `predecessor` and `successor` are controlled by the same entity.
    pub fn valid_successor(
        &self,
        predecessor: &CertificateChain,
        successor: &CertificateChain,
    ) -> Result<bool, X509IdentityError> {
        let predecessor_cert = get_certificate(predecessor, 0)?;
        let successor_cert = get_certificate(successor, 0)?;

        let predecessor_common_name = self.extract_common_name(predecessor_cert)?;

        let successor_common_name = self.extract_common_name(successor_cert)?;

        if let (Some(pre_common_name), Some(succ_common_name)) =
            (predecessor_common_name, successor_common_name)
        {
            return Ok(pre_common_name == succ_common_name);
        }

        Ok(self.subject_bytes(predecessor_cert)? == self.subject_bytes(successor_cert)?)
    }
}

impl<R> X509IdentityExtractor for SubjectIdentityExtractor<R>
where
    R: X509CertificateReader,
{
    type Error = X509IdentityError;

    fn identity(&self, certificate_chain: &CertificateChain) -> Result<Vec<u8>, Self::Error> {
        self.identity(certificate_chain)
    }

    fn valid_successor(
        &self,
        predecessor: &CertificateChain,
        successor: &CertificateChain,
    ) -> Result<bool, Self::Error> {
        self.valid_successor(predecessor, successor)
    }
}

fn get_certificate(
    certificate_chain: &CertificateChain,
    offset: usize,
) -> Result<&DerCertificate, X509IdentityError> {
    certificate_chain
        .get(offset)
        .ok_or(X509IdentityError::InvalidOffset)
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use crate::{
        test_utils::test_certificate_chain, MockX509CertificateReader, SubjectComponent,
        SubjectIdentityExtractor, X509IdentityError,
    };

    use alloc::vec;
    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    fn test_setup<F>(
        offset: usize,
        mut mock_setup: F,
    ) -> SubjectIdentityExtractor<MockX509CertificateReader>
    where
        F: FnMut(&mut MockX509CertificateReader),
    {
        let mut x509_reader = MockX509CertificateReader::new();

        mock_setup(&mut x509_reader);

        SubjectIdentityExtractor {
            offset,
            reader: x509_reader,
        }
    }

    #[test]
    fn invalid_offset_is_rejected() {
        let subject_extractor = test_setup(4, |subject_extractor| {
            subject_extractor.expect_subject_bytes().never();
        });

        assert_matches!(
            subject_extractor.identity(&test_certificate_chain()),
            Err(X509IdentityError::InvalidOffset)
        );
    }

    #[test]
    fn common_name_can_be_retrived_as_identity() {
        let test_subject = b"test_name".to_vec();
        let cert_chain = test_certificate_chain();

        let expected_certificate = cert_chain[1].clone();

        let subject_extractor = test_setup(1, |parser| {
            parser.expect_subject_bytes().never();

            parser
                .expect_subject_components()
                .with(mockall::predicate::eq(expected_certificate.clone()))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![
                        SubjectComponent::CommonName("test_name".to_string()),
                        SubjectComponent::CountryName("US".to_string()),
                    ])
                });
        });

        assert_eq!(
            subject_extractor.identity(&cert_chain).unwrap(),
            test_subject
        );
    }

    #[test]
    fn subject_can_be_retrived_as_identity_if_no_common_name() {
        let test_subject = b"subject".to_vec();
        let cert_chain = test_certificate_chain();

        let expected_certificate = cert_chain[1].clone();

        let subject_extractor = test_setup(1, |parser| {
            let test_subject = test_subject.clone();

            parser
                .expect_subject_bytes()
                .once()
                .with(mockall::predicate::eq(expected_certificate.clone()))
                .return_once_st(|_| Ok(test_subject));

            parser
                .expect_subject_components()
                .with(mockall::predicate::eq(expected_certificate.clone()))
                .times(1)
                .return_once_st(|_| Ok(vec![SubjectComponent::CountryName("US".to_string())]));
        });

        assert_eq!(
            subject_extractor.identity(&cert_chain).unwrap(),
            test_subject
        );
    }

    #[test]
    fn valid_successor_matching_common_name() {
        let predecessor = test_certificate_chain();
        let mut successor = test_certificate_chain();

        // Make sure both chains have the same leaf
        successor[0] = predecessor[0].clone();

        let subject_extractor = test_setup(1, |reader| {
            let predecessor = predecessor[0].clone();
            let successor = successor[0].clone();

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(successor))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![SubjectComponent::CommonName("test_name".to_string())])
                });

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(predecessor))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![SubjectComponent::CommonName("test_name".to_string())])
                });

            reader.expect_subject_bytes().never();

            reader.expect_subject_bytes().never();
        });

        assert!(subject_extractor
            .valid_successor(&predecessor, &successor)
            .unwrap());
    }

    #[test]
    fn invalid_successor_different_common_name() {
        let predecessor = test_certificate_chain();
        let mut successor = test_certificate_chain();

        // Make sure both chains have the same leaf
        successor[0] = predecessor[0].clone();

        let subject_extractor = test_setup(1, |reader| {
            let predecessor = predecessor[0].clone();
            let successor = successor[0].clone();

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(successor))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![
                        SubjectComponent::CommonName("test_name_copy".to_string()),
                        SubjectComponent::CountryName("US".to_string()),
                    ])
                });

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(predecessor))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![
                        SubjectComponent::CommonName("test_name".to_string()),
                        SubjectComponent::CountryName("US".to_string()),
                    ])
                });

            reader.expect_subject_bytes().never();

            reader.expect_subject_bytes().never();
        });

        assert!(
            !subject_extractor
                .valid_successor(&predecessor, &successor)
                .unwrap(),
            "Successor chain cert with different CommonName passed check!"
        );
    }

    #[test]
    fn valid_successor_no_common_name() {
        let predecessor = test_certificate_chain();
        let mut successor = test_certificate_chain();

        // Make sure both chains have the same leaf
        successor[0] = predecessor[0].clone();

        let subject_extractor = test_setup(1, |reader| {
            let predecessor = predecessor[0].clone();
            let successor = successor[0].clone();

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(successor.clone()))
                .times(1)
                .return_once_st(|_| Ok(vec![SubjectComponent::CountryName("US".to_string())]));

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(predecessor.clone()))
                .times(1)
                .return_once_st(|_| {
                    Ok(vec![
                        SubjectComponent::CommonName("test_name".to_string()),
                        SubjectComponent::CountryName("US".to_string()),
                    ])
                });

            reader
                .expect_subject_bytes()
                .with(mockall::predicate::eq(predecessor))
                .times(1)
                .return_once_st(|_| Ok(b"subject".to_vec()));

            reader
                .expect_subject_bytes()
                .with(mockall::predicate::eq(successor))
                .times(1)
                .return_once_st(|_| Ok(b"subject".to_vec()));
        });

        assert!(subject_extractor
            .valid_successor(&predecessor, &successor)
            .unwrap());
    }

    #[test]
    fn invalid_successor_no_common_name() {
        let predecessor = test_certificate_chain();
        let mut successor = test_certificate_chain();

        // Make sure both chains have the same leaf
        successor[0] = predecessor[0].clone();

        let subject_extractor = test_setup(1, |reader| {
            let predecessor = predecessor[0].clone();
            let successor = successor[0].clone();

            reader
                .expect_subject_bytes()
                .with(mockall::predicate::eq(predecessor.clone()))
                .times(1)
                .return_once_st(|_| Ok(b"subject_copy".to_vec()));

            reader
                .expect_subject_bytes()
                .with(mockall::predicate::eq(successor.clone()))
                .times(1)
                .return_once_st(|_| Ok(b"subject".to_vec()));

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(successor))
                .times(1)
                .return_once_st(|_| Ok(vec![SubjectComponent::CountryName("US".to_string())]));

            reader
                .expect_subject_components()
                .with(mockall::predicate::eq(predecessor))
                .times(1)
                .return_once_st(|_| Ok(vec![SubjectComponent::CountryName("US".to_string())]));
        });

        assert!(
            !subject_extractor
                .valid_successor(&predecessor, &successor)
                .unwrap(),
            "Successor cert chain with different subjects passed valid check!"
        );
    }
}
