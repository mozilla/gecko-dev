/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use firefox_versioning::error::VersionParsingError;
use firefox_versioning::version::{Version, VersionPart};

pub type Result<T, E = VersionParsingError> = std::result::Result<T, E>;

#[test]
fn test_wild_card_to_version_part() -> Result<()> {
    let s = "*";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, i32::MAX);
    assert_eq!(version_part.str_b, "");
    assert_eq!(version_part.num_c, 0);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_empty_string_to_version_part() -> Result<()> {
    let s = "";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 0);
    assert_eq!(version_part.str_b, "");
    assert_eq!(version_part.num_c, 0);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_only_num_a_to_version_part() -> Result<()> {
    let s = "98382";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 98382);
    assert_eq!(version_part.str_b, "");
    assert_eq!(version_part.num_c, 0);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_num_a_and_plus_str_b() -> Result<()> {
    let s = "92+";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 93);
    assert_eq!(version_part.str_b, "pre");
    assert_eq!(version_part.num_c, 0);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_num_a_and_str_b() -> Result<()> {
    let s = "92beta";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 92);
    assert_eq!(version_part.str_b, "beta");
    assert_eq!(version_part.num_c, 0);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_num_a_str_b_and_num_c() -> Result<()> {
    let s = "92beta72";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 92);
    assert_eq!(version_part.str_b, "beta");
    assert_eq!(version_part.num_c, 72);
    assert_eq!(version_part.extra_d, "");
    Ok(())
}

#[test]
fn test_full_valid_string_to_version_part() -> Result<()> {
    let s = "1pre3extrabithere";
    let version_part = VersionPart::try_from(s)?;
    assert_eq!(version_part.num_a, 1);
    assert_eq!(version_part.str_b, "pre");
    assert_eq!(version_part.num_c, 3);
    assert_eq!(version_part.extra_d, "extrabithere");
    Ok(())
}

#[test]

fn test_parse_full_version() -> Result<()> {
    let s = "92+.10.1.beta";
    let versions = Version::try_from(s.to_string())?;
    // assert!(versions.ok(), true);
    assert_eq!(
        vec![
            VersionPart {
                num_a: 93,
                str_b: "pre".into(),
                ..Default::default()
            },
            VersionPart {
                num_a: 10,
                ..Default::default()
            },
            VersionPart {
                num_a: 1,
                ..Default::default()
            },
            VersionPart {
                num_a: 0,
                str_b: "beta".into(),
                ..Default::default()
            }
        ],
        versions.0
    );
    Ok(())
}

#[test]
fn test_compare_two_versions() -> Result<()> {
    let v1 = Version::try_from("92beta.1.2".to_string())?;
    let v2 = Version::try_from("92beta.1.2pre".to_string())?;
    assert!(v1 > v2);
    Ok(())
}

#[test]
fn smoke_test_version_compare() -> Result<()> {
    // Test values from https://searchfox.org/mozilla-central/rev/5909d5b7f3e247dddff8229e9499db017eb438e2/xpcom/base/nsIVersionComparator.idl#24-31
    let v1 = Version::try_from("1.0pre1")?;
    let v2 = Version::try_from("1.0pre2")?;
    let v3 = Version::try_from("1.0")?;
    let v4 = Version::try_from("1.0.0")?;
    let v5 = Version::try_from("1.0.0.0")?;
    let v6 = Version::try_from("1.1pre")?;
    let v7 = Version::try_from("1.1pre0")?;
    let v8 = Version::try_from("1.0+")?;
    let v9 = Version::try_from("1.1pre1a")?;
    let v10 = Version::try_from("1.1pre1")?;
    let v11 = Version::try_from("1.1pre10a")?;
    let v12 = Version::try_from("1.1pre10")?;
    assert!(v1 < v2);
    assert!(v2 < v3);
    assert!(v3 == v4);
    assert!(v4 == v5);
    assert!(v5 < v6);
    assert!(v6 == v7);
    assert!(v7 == v8);
    assert!(v8 < v9);
    assert!(v9 < v10);
    assert!(v10 < v11);
    assert!(v11 < v12);
    Ok(())
}

#[test]
fn test_compare_wild_card() -> Result<()> {
    let v1 = Version::try_from("*")?;
    let v2 = Version::try_from("95.2pre")?;
    assert!(v1 > v2);
    Ok(())
}

#[test]
fn test_non_ascii_throws_error() -> Result<()> {
    let err = Version::try_from("92🥲.1.2pre").expect_err("Should have thrown error");
    if let VersionParsingError::ParseError(_) = err {
        // Good!
    } else {
        panic!("Expected VersionParsingError, got {:?}", err)
    }
    Ok(())
}

#[test]
fn test_version_number_parsing_overflows() -> Result<()> {
    // This i32::MAX, should parse OK
    let v1 = VersionPart::try_from("2147483647")?;
    assert_eq!(v1.num_a, i32::MAX);
    // this is greater than i32::MAX, should return an error
    let err =
        VersionPart::try_from("2147483648").expect_err("Should throw error, it overflows an i32");
    if let VersionParsingError::Overflow(_) = err {
        // OK
    } else {
        panic!("Expected a VersionParsingError, got {:?}", err)
    }
    Ok(())
}

#[test]
fn test_version_part_with_dashes() -> Result<()> {
    let v1 = VersionPart::try_from("0-beta")?;
    assert_eq!(
        VersionPart {
            num_a: 0,
            str_b: "".into(),
            num_c: 0,
            extra_d: "-beta".into(),
        },
        v1
    );
    Ok(())
}

#[test]
fn test_exclamation_mark() -> Result<()> {
    let v1 = Version::try_from("93.!")?;
    let v2 = Version::try_from("93.1")?;
    let v3 = Version::try_from("93.0-beta")?;
    let v4 = Version::try_from("93.alpha")?;
    assert!(v1 < v2 && v1 < v3 && v1 < v4);
    Ok(())
}
