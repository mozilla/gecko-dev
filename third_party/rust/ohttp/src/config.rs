use crate::{
    err::{Error, Res},
    hpke::{Aead as AeadId, Kdf, Kem},
    KeyId,
};
use byteorder::{NetworkEndian, ReadBytesExt, WriteBytesExt};
use std::{
    convert::TryFrom,
    io::{BufRead, BufReader, Cursor, Read},
};

#[cfg(feature = "nss")]
use crate::nss::{
    hpke::{generate_key_pair, Config as HpkeConfig, HpkeR},
    PrivateKey, PublicKey,
};

#[cfg(feature = "rust-hpke")]
use crate::rh::hpke::{
    derive_key_pair, generate_key_pair, Config as HpkeConfig, HpkeR, PrivateKey, PublicKey,
};

/// A tuple of KDF and AEAD identifiers.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct SymmetricSuite {
    kdf: Kdf,
    aead: AeadId,
}

impl SymmetricSuite {
    #[must_use]
    pub const fn new(kdf: Kdf, aead: AeadId) -> Self {
        Self { kdf, aead }
    }

    #[must_use]
    pub fn kdf(self) -> Kdf {
        self.kdf
    }

    #[must_use]
    pub fn aead(self) -> AeadId {
        self.aead
    }
}

/// The key configuration of a server.  This can be used by both client and server.
/// An important invariant of this structure is that it does not include
/// any combination of KEM, KDF, and AEAD that is not supported.
#[allow(clippy::module_name_repetitions)]
#[derive(Debug, Clone)]
pub struct KeyConfig {
    pub(crate) key_id: KeyId,
    pub(crate) kem: Kem,
    pub(crate) symmetric: Vec<SymmetricSuite>,
    pub(crate) sk: Option<PrivateKey>,
    pub(crate) pk: PublicKey,
}

impl KeyConfig {
    fn strip_unsupported(symmetric: &mut Vec<SymmetricSuite>, kem: Kem) {
        symmetric.retain(|s| HpkeConfig::new(kem, s.kdf(), s.aead()).supported());
    }

    /// Construct a configuration for the server side.
    /// # Panics
    /// If the configurations don't include a supported configuration.
    pub fn new(key_id: u8, kem: Kem, mut symmetric: Vec<SymmetricSuite>) -> Res<Self> {
        Self::strip_unsupported(&mut symmetric, kem);
        assert!(!symmetric.is_empty());
        let (sk, pk) = generate_key_pair(kem)?;
        Ok(Self {
            key_id,
            kem,
            symmetric,
            sk: Some(sk),
            pk,
        })
    }

    /// Derive a configuration for the server side from input keying material,
    /// using the `DeriveKeyPair` functionality of the HPKE KEM defined here:
    /// <https://www.ietf.org/archive/id/draft-irtf-cfrg-hpke-12.html#section-4>
    /// # Panics
    /// If the configurations don't include a supported configuration.
    #[allow(unused)]
    pub fn derive(
        key_id: u8,
        kem: Kem,
        mut symmetric: Vec<SymmetricSuite>,
        ikm: &[u8],
    ) -> Res<Self> {
        #[cfg(feature = "rust-hpke")]
        {
            Self::strip_unsupported(&mut symmetric, kem);
            assert!(!symmetric.is_empty());
            let (sk, pk) = derive_key_pair(kem, ikm)?;
            Ok(Self {
                key_id,
                kem,
                symmetric,
                sk: Some(sk),
                pk,
            })
        }
        #[cfg(not(feature = "rust-hpke"))]
        {
            Err(Error::Unsupported)
        }
    }

    /// Encode a list of key configurations.
    ///
    /// This produces the key configuration format that is used for
    /// the "application/ohttp-keys" media type.
    /// Each item in the list is written as per [`encode()`].
    ///
    /// # Panics
    /// Not as a result of this function.
    ///
    /// [`encode()`]: Self::encode
    pub fn encode_list(list: &[impl AsRef<Self>]) -> Res<Vec<u8>> {
        let mut buf = Vec::new();
        for c in list {
            let offset = buf.len();
            buf.write_u16::<NetworkEndian>(0)?;
            c.as_ref().write(&mut buf)?;
            let len = buf.len() - offset - 2;
            buf[offset] = u8::try_from(len >> 8)?;
            buf[offset + 1] = u8::try_from(len & 0xff).unwrap();
        }
        Ok(buf)
    }

    fn write(&self, buf: &mut Vec<u8>) -> Res<()> {
        buf.write_u8(self.key_id)?;
        buf.write_u16::<NetworkEndian>(u16::from(self.kem))?;
        let pk_buf = self.pk.key_data()?;
        buf.extend_from_slice(&pk_buf);
        buf.write_u16::<NetworkEndian>((self.symmetric.len() * 4).try_into()?)?;
        for s in &self.symmetric {
            buf.write_u16::<NetworkEndian>(u16::from(s.kdf()))?;
            buf.write_u16::<NetworkEndian>(u16::from(s.aead()))?;
        }
        Ok(())
    }

    /// Encode into a wire format.  This shares a format with the core of ECH:
    ///
    /// ```tls-format
    /// opaque HpkePublicKey[Npk];
    /// uint16 HpkeKemId;  // Defined in I-D.irtf-cfrg-hpke
    /// uint16 HpkeKdfId;  // Defined in I-D.irtf-cfrg-hpke
    /// uint16 HpkeAeadId; // Defined in I-D.irtf-cfrg-hpke
    ///
    /// struct {
    ///   HpkeKdfId kdf_id;
    ///   HpkeAeadId aead_id;
    /// } ECHCipherSuite;
    ///
    /// struct {
    ///   uint8 key_id;
    ///   HpkeKemId kem_id;
    ///   HpkePublicKey public_key;
    ///   ECHCipherSuite cipher_suites<4..2^16-4>;
    /// } ECHKeyConfig;
    /// ```
    /// # Panics
    /// Not as a result of this function.
    pub fn encode(&self) -> Res<Vec<u8>> {
        let mut buf = Vec::new();
        self.write(&mut buf)?;
        Ok(buf)
    }

    /// Construct a configuration from the encoded server configuration.
    /// The format of `encoded_config` is the output of `Self::encode`.
    pub fn decode(encoded_config: &[u8]) -> Res<Self> {
        let end_position = u64::try_from(encoded_config.len())?;
        let mut r = Cursor::new(encoded_config);
        let key_id = r.read_u8()?;
        let kem = Kem::try_from(r.read_u16::<NetworkEndian>()?)?;

        // Note that the KDF and AEAD doesn't matter here.
        let kem_config = HpkeConfig::new(kem, Kdf::HkdfSha256, AeadId::Aes128Gcm);
        if !kem_config.supported() {
            return Err(Error::Unsupported);
        }
        let mut pk_buf = vec![0; kem_config.kem().n_pk()];
        r.read_exact(&mut pk_buf)?;

        let sym_len = r.read_u16::<NetworkEndian>()?;
        let mut sym = vec![0; usize::from(sym_len)];
        r.read_exact(&mut sym)?;
        if sym.is_empty() || (sym.len() % 4 != 0) {
            return Err(Error::Format);
        }
        let sym_count = sym.len() / 4;
        let mut sym_r = BufReader::new(&sym[..]);
        let mut symmetric = Vec::with_capacity(sym_count);
        for _ in 0..sym_count {
            let kdf = Kdf::try_from(sym_r.read_u16::<NetworkEndian>()?)?;
            let aead = AeadId::try_from(sym_r.read_u16::<NetworkEndian>()?)?;
            symmetric.push(SymmetricSuite::new(kdf, aead));
        }

        // Check that there was nothing extra and we are at the end of the buffer.
        if r.position() != end_position {
            return Err(Error::Format);
        }

        Self::strip_unsupported(&mut symmetric, kem);
        let pk = HpkeR::decode_public_key(kem_config.kem(), &pk_buf)?;

        Ok(Self {
            key_id,
            kem,
            symmetric,
            sk: None,
            pk,
        })
    }

    /// Decode a list of key configurations.
    /// This only returns the valid and supported key configurations;
    /// unsupported configurations are dropped silently.
    pub fn decode_list(encoded_list: &[u8]) -> Res<Vec<Self>> {
        let end_position = u64::try_from(encoded_list.len())?;
        let mut r = Cursor::new(encoded_list);
        let mut configs = Vec::new();
        loop {
            if r.position() == end_position {
                break;
            }
            let len = usize::from(r.read_u16::<NetworkEndian>()?);
            let buf = r.fill_buf()?;
            if len > buf.len() {
                return Err(Error::Truncated);
            }
            let res = Self::decode(&buf[..len]);
            r.consume(len);
            match res {
                Ok(config) => configs.push(config),
                Err(Error::Unsupported) => continue,
                Err(e) => return Err(e),
            }
        }
        Ok(configs)
    }

    /// Select creates a new configuration that contains the identified symmetric suite.
    ///
    /// # Errors
    /// If the given suite is not supported by this configuration.
    pub fn select(&self, sym: SymmetricSuite) -> Res<HpkeConfig> {
        if self.symmetric.contains(&sym) {
            let config = HpkeConfig::new(self.kem, sym.kdf(), sym.aead());
            Ok(config)
        } else {
            Err(Error::Unsupported)
        }
    }
}

impl AsRef<Self> for KeyConfig {
    fn as_ref(&self) -> &Self {
        self
    }
}

#[cfg(test)]
mod test {
    use crate::{
        hpke::{Aead, Kdf, Kem},
        init, Error, KeyConfig, KeyId, SymmetricSuite,
    };
    use std::iter::zip;

    const KEY_ID: KeyId = 1;
    const KEM: Kem = Kem::X25519Sha256;
    const SYMMETRIC: &[SymmetricSuite] = &[
        SymmetricSuite::new(Kdf::HkdfSha256, Aead::Aes128Gcm),
        SymmetricSuite::new(Kdf::HkdfSha256, Aead::ChaCha20Poly1305),
    ];

    #[test]
    fn encode_decode_config_list() {
        const COUNT: usize = 3;
        init();

        let mut configs = Vec::with_capacity(COUNT);
        configs.resize_with(COUNT, || {
            KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC)).unwrap()
        });

        let buf = KeyConfig::encode_list(&configs).unwrap();
        let decoded_list = KeyConfig::decode_list(&buf).unwrap();
        for (original, decoded) in zip(&configs, &decoded_list) {
            assert_eq!(decoded.key_id, original.key_id);
            assert_eq!(decoded.kem, original.kem);
            assert_eq!(
                decoded.pk.key_data().unwrap(),
                original.pk.key_data().unwrap()
            );
            assert!(decoded.sk.is_none());
            assert!(original.sk.is_some());
        }

        // Check that truncation errors in `KeyConfig::decode` are caught.
        assert!(KeyConfig::decode_list(&buf[..buf.len() - 3]).is_err());
    }

    #[test]
    fn empty_config_list() {
        let list = KeyConfig::decode_list(&[]).unwrap();
        assert!(list.is_empty());

        // A reserved KEM ID is not bad.  Note that we don't check that the data
        // following the KEM ID is even the minimum length, allowing this to be
        // zero bytes, where you need at least some bytes in a public key and some
        // bytes to identify at least one KDF and AEAD (i.e., more than 6 bytes).
        let list = KeyConfig::decode_list(&[0, 3, 0, 0, 0]).unwrap();
        assert!(list.is_empty());
    }

    #[test]
    fn bad_config_list_length() {
        init();

        // A one byte length for a config.
        let res = KeyConfig::decode_list(&[0]);
        assert!(matches!(res, Err(Error::Io(_))));
    }

    #[test]
    fn decode_bad_config() {
        init();

        let mut x25519 = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC))
            .unwrap()
            .encode()
            .unwrap();
        {
            // Truncation tests.
            let trunc = |n: usize| KeyConfig::decode(&x25519[..n]);

            // x25519, truncated inside the KEM ID.
            assert!(matches!(trunc(2), Err(Error::Io(_))));
            // ... inside the public key.
            assert!(matches!(trunc(4), Err(Error::Io(_))));
            // ... inside the length of the KDF+AEAD list.
            assert!(matches!(trunc(36), Err(Error::Io(_))));
            // ... inside the KDF+AEAD list.
            assert!(matches!(trunc(38), Err(Error::Io(_))));
        }

        // And then with an extra byte at the end.
        x25519.push(0);
        assert!(matches!(KeyConfig::decode(&x25519), Err(Error::Format)));
    }

    /// Truncate the KDF+AEAD list badly.
    #[test]
    fn truncate_kdf_aead_list() {
        init();

        let mut x25519 = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC))
            .unwrap()
            .encode()
            .unwrap();
        x25519.truncate(38);
        assert_eq!(usize::from(x25519[36]), SYMMETRIC.len() * 4);
        x25519[36] = 1;
        assert!(matches!(KeyConfig::decode(&x25519), Err(Error::Format)));
    }
}
