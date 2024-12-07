use crate::Error;

pub const TAG_INTEGER: u8 = 0x02;
pub const TAG_BIT_STRING: u8 = 0x03;
pub const TAG_OCTET_STRING: u8 = 0x04;
pub const TAG_NULL: u8 = 0x05;
pub const TAG_OBJECT_ID: u8 = 0x06;
pub const TAG_SEQUENCE: u8 = 0x30;

// Object identifiers in DER tag-length-value form
pub const OID_EC_PUBLIC_KEY_BYTES: &[u8] = &[
    /* RFC 5480 (id-ecPublicKey) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
];
pub const OID_SECP256R1_BYTES: &[u8] = &[
    /* RFC 5480 (secp256r1) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
];
pub const OID_ED25519_BYTES: &[u8] = &[/* RFC 8410 (id-ed25519) */ 0x2b, 0x65, 0x70];
pub const OID_RS256_BYTES: &[u8] = &[
    /* RFC 4055 (sha256WithRSAEncryption) */
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
];

pub const MAX_TAG_AND_LENGTH_BYTES: usize = 4;

pub fn write_tag_and_length(out: &mut Vec<u8>, tag: u8, len: usize) -> Result<(), crate::Error> {
    if len > 0xFFFF {
        return Err(crate::Error::InternalError);
    }
    out.push(tag);
    if len > 0xFF {
        out.push(0x82);
        out.push((len >> 8) as u8);
        out.push(len as u8);
    } else if len > 0x7F {
        out.push(0x81);
        out.push(len as u8);
    } else {
        out.push(len as u8);
    }
    Ok(())
}

pub fn integer(val: &[u8]) -> Result<Vec<u8>, crate::Error> {
    if val.is_empty() {
        return Err(crate::Error::InvalidInput);
    }
    // trim leading zeros, leaving a single zero if the input is the zero vector.
    let mut val = val;
    while val.len() > 1 && val[0] == 0 {
        val = &val[1..];
    }
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + 1 + val.len());
    if val[0] & 0x80 != 0 {
        // needs zero prefix
        write_tag_and_length(&mut out, TAG_INTEGER, 1 + val.len())?;
        out.push(0x00);
        out.extend_from_slice(val);
    } else {
        write_tag_and_length(&mut out, TAG_INTEGER, val.len())?;
        out.extend_from_slice(val);
    }
    Ok(out)
}

pub fn bit_string(val: &[u8]) -> Result<Vec<u8>, crate::Error> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + 1 + val.len());
    write_tag_and_length(&mut out, TAG_BIT_STRING, 1 + val.len())?;
    out.push(0x00); // trailing bits aren't supported
    out.extend_from_slice(val);
    Ok(out)
}

pub fn null() -> Result<Vec<u8>, crate::Error> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES);
    write_tag_and_length(&mut out, TAG_NULL, 0)?;
    Ok(out)
}

pub fn object_id(val: &[u8]) -> Result<Vec<u8>, crate::Error> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + val.len());
    write_tag_and_length(&mut out, TAG_OBJECT_ID, val.len())?;
    out.extend_from_slice(val);
    Ok(out)
}

pub fn sequence(items: &[&[u8]]) -> Result<Vec<u8>, crate::Error> {
    let len = items.iter().map(|i| i.len()).sum();
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + len);
    write_tag_and_length(&mut out, TAG_SEQUENCE, len)?;
    for item in items {
        out.extend_from_slice(item);
    }
    Ok(out)
}

pub fn octet_string(val: &[u8]) -> Result<Vec<u8>, crate::Error> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + val.len());
    write_tag_and_length(&mut out, TAG_OCTET_STRING, val.len())?;
    out.extend_from_slice(val);
    Ok(out)
}

pub fn context_specific_explicit_tag(tag: u8, content: &[u8]) -> Result<Vec<u8>, crate::Error> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + content.len());
    write_tag_and_length(&mut out, 0xa0 + tag, content.len())?;
    out.extend_from_slice(content);
    Ok(out)
}

// Given "tag || len || value || rest" where tag and len are of length one, len is in [0, 127],
// and value is of length len, returns (value, rest)
pub fn expect_tag_with_short_len(tag: u8, z: &[u8]) -> Result<(&[u8], &[u8]), crate::Error> {
    if z.is_empty() {
        return Err(Error::InvalidInput);
    }
    let (h, z) = z.split_at(1);
    if h[0] != tag || z.is_empty() {
        return Err(Error::InvalidInput);
    }
    let (h, z) = z.split_at(1);
    if h[0] >= 0x80 || h[0] as usize > z.len() {
        return Err(Error::InvalidInput);
    }
    Ok(z.split_at(h[0] as usize))
}
