/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clubcard_crlite::{CRLiteClubcard, CRLiteKey, CRLiteStatus};
use sha2::{Digest, Sha256};
use std::env::args;
use std::path::PathBuf;
use std::process::ExitCode;
use x509_parser::prelude::*;

fn read_as_der(path: &PathBuf) -> Result<Vec<u8>, std::io::Error> {
    let bytes = std::fs::read(&path)?;
    match parse_x509_pem(&bytes) {
        Ok((_, pem)) => Ok(pem.contents),
        _ => Ok(bytes),
    }
}

fn parse_args() -> Option<(PathBuf, PathBuf, PathBuf)> {
    let mut args = args().map(PathBuf::from);
    let _name = args.next()?;
    Some((args.next()?, args.next()?, args.next()?))
}

fn main() -> std::process::ExitCode {
    let Some((filter_path, issuer_cert_path, end_entity_cert_path)) = parse_args() else {
        eprintln!(
            "Usage: {} <filter> <issuer certificate> <end entity certificate>",
            args().next().unwrap()
        );
        return ExitCode::FAILURE;
    };

    let Ok(filter_bytes) = std::fs::read(&filter_path) else {
        eprintln!("Could not read filter");
        return ExitCode::FAILURE;
    };

    let Ok(filter) = CRLiteClubcard::from_bytes(&filter_bytes) else {
        eprintln!("Could not parse filter");
        return ExitCode::FAILURE;
    };

    let Ok(issuer_bytes) = read_as_der(&issuer_cert_path) else {
        eprintln!("Could not read issuer certificate");
        return ExitCode::FAILURE;
    };

    let Ok((_, issuer)) = X509Certificate::from_der(&issuer_bytes) else {
        eprintln!("Could not parse issuer certificate");
        return ExitCode::FAILURE;
    };

    let Ok(cert_bytes) = read_as_der(&end_entity_cert_path) else {
        eprintln!("Could not read end-entity certificate");
        return ExitCode::FAILURE;
    };

    let Ok((_, cert)) = X509Certificate::from_der(&cert_bytes) else {
        eprintln!("Could not parse end-entity certificate");
        return ExitCode::FAILURE;
    };

    if cert.verify_signature(Some(issuer.public_key())).is_err() {
        eprintln!("Invalid signature (wrong issuer certificate?)");
        return ExitCode::FAILURE;
    }

    if !cert.tbs_certificate.validity.is_valid() {
        eprintln!("End-entity certificate is expired");
        return ExitCode::FAILURE;
    }

    let Ok(Some(sct_extension)) = cert
        .tbs_certificate
        .get_extension_unique(&x509_parser::oid_registry::OID_CT_LIST_SCT)
    else {
        eprintln!("End entity certificate has no SCTs");
        return ExitCode::FAILURE;
    };

    let ParsedExtension::SCT(scts) = sct_extension.parsed_extension() else {
        eprintln!("End entity certificate has no SCTs");
        return ExitCode::FAILURE;
    };

    let issuer_spki_hash: [u8; 32] = Sha256::digest(issuer.tbs_certificate.subject_pki.raw).into();
    let serial = cert.tbs_certificate.raw_serial();
    let key = CRLiteKey::new(&issuer_spki_hash, &serial);

    match filter.contains(&key, scts.iter().map(|sct| (sct.id.key_id, sct.timestamp))) {
        CRLiteStatus::Good => println!("Good"),
        CRLiteStatus::Revoked => println!("Revoked"),
        CRLiteStatus::NotEnrolled | CRLiteStatus::NotCovered => println!("Unknown"),
    };

    ExitCode::SUCCESS
}
