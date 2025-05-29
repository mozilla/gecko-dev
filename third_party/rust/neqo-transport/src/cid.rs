// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Representation and management of connection IDs.

use std::{
    borrow::Borrow,
    cell::{Ref, RefCell},
    cmp::{max, min},
    fmt::{self, Debug, Display, Formatter},
    ops::Deref,
    rc::Rc,
};

use neqo_common::{hex, hex_with_len, qdebug, qinfo, Decoder, Encoder};
use neqo_crypto::{random, randomize};
use smallvec::{smallvec, SmallVec};

use crate::{
    frame::FrameType, packet::PacketBuilder, recovery::RecoveryToken, stats::FrameStats, Error, Res,
};

pub const MAX_CONNECTION_ID_LEN: usize = 20;
pub const LOCAL_ACTIVE_CID_LIMIT: usize = 8;
pub const CONNECTION_ID_SEQNO_INITIAL: u64 = 0;
pub const CONNECTION_ID_SEQNO_PREFERRED: u64 = 1;
/// A special value.  See `ConnectionIdManager::add_odcid`.
const CONNECTION_ID_SEQNO_ODCID: u64 = u64::MAX;
/// A special value.  See `ConnectionIdEntry::empty_remote`.
const CONNECTION_ID_SEQNO_EMPTY: u64 = u64::MAX - 1;

#[derive(Clone, Default, Eq, Hash, PartialEq)]
pub struct ConnectionId {
    cid: SmallVec<[u8; MAX_CONNECTION_ID_LEN]>,
}

impl ConnectionId {
    /// # Panics
    /// When `len` is larger than `MAX_CONNECTION_ID_LEN`.
    #[must_use]
    pub fn generate(len: usize) -> Self {
        assert!(matches!(len, 0..=MAX_CONNECTION_ID_LEN));
        let mut cid = smallvec![0; len];
        randomize(&mut cid);
        Self { cid }
    }

    // Apply a wee bit of greasing here in picking a length between 8 and 20 bytes long.
    #[must_use]
    pub fn generate_initial() -> Self {
        let v = random::<1>()[0];
        // Bias selection toward picking 8 (>50% of the time).
        let len: usize = max(8, 5 + (v & (v >> 4))).into();
        Self::generate(len)
    }

    #[must_use]
    pub fn as_cid_ref(&self) -> ConnectionIdRef {
        ConnectionIdRef::from(&self.cid[..])
    }
}

impl AsRef<[u8]> for ConnectionId {
    fn as_ref(&self) -> &[u8] {
        self.borrow()
    }
}

impl Borrow<[u8]> for ConnectionId {
    fn borrow(&self) -> &[u8] {
        &self.cid
    }
}

impl From<SmallVec<[u8; MAX_CONNECTION_ID_LEN]>> for ConnectionId {
    fn from(cid: SmallVec<[u8; MAX_CONNECTION_ID_LEN]>) -> Self {
        Self { cid }
    }
}

impl<T: AsRef<[u8]> + ?Sized> From<&T> for ConnectionId {
    fn from(buf: &T) -> Self {
        Self::from(SmallVec::from_slice(buf.as_ref()))
    }
}

impl<'a> From<ConnectionIdRef<'a>> for ConnectionId {
    fn from(cidref: ConnectionIdRef<'a>) -> Self {
        Self::from(SmallVec::from_slice(cidref.cid))
    }
}

impl Deref for ConnectionId {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.cid
    }
}

impl Debug for ConnectionId {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "CID {}", hex_with_len(&self.cid))
    }
}

impl Display for ConnectionId {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", hex(&self.cid))
    }
}

impl<'a> PartialEq<ConnectionIdRef<'a>> for ConnectionId {
    fn eq(&self, other: &ConnectionIdRef<'a>) -> bool {
        &self.cid[..] == other.cid
    }
}

#[derive(Hash, Eq, PartialEq, Clone, Copy)]
pub struct ConnectionIdRef<'a> {
    cid: &'a [u8],
}

impl Debug for ConnectionIdRef<'_> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "CID {}", hex_with_len(self.cid))
    }
}

impl Display for ConnectionIdRef<'_> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", hex(self.cid))
    }
}

impl<'a, T: AsRef<[u8]> + ?Sized> From<&'a T> for ConnectionIdRef<'a> {
    fn from(cid: &'a T) -> Self {
        Self { cid: cid.as_ref() }
    }
}

impl Deref for ConnectionIdRef<'_> {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        self.cid
    }
}

impl PartialEq<ConnectionId> for ConnectionIdRef<'_> {
    fn eq(&self, other: &ConnectionId) -> bool {
        self.cid == &other.cid[..]
    }
}

pub trait ConnectionIdDecoder {
    /// Decodes a connection ID from the provided decoder.
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>>;
}

pub trait ConnectionIdGenerator: ConnectionIdDecoder {
    /// Generates a connection ID.  This can return `None` if the generator
    /// is exhausted.
    fn generate_cid(&mut self) -> Option<ConnectionId>;
    /// Indicates whether the connection IDs are zero-length.
    /// If this returns true, `generate_cid` must always produce an empty value
    /// and never `None`.
    /// If this returns false, `generate_cid` must never produce an empty value,
    /// though it can return `None`.
    ///
    /// You should not need to implement this: if you want zero-length connection IDs,
    /// use `EmptyConnectionIdGenerator` instead.
    fn generates_empty_cids(&self) -> bool {
        false
    }
    fn as_decoder(&self) -> &dyn ConnectionIdDecoder;
}

/// An `EmptyConnectionIdGenerator` generates empty connection IDs.
#[derive(Default)]
pub struct EmptyConnectionIdGenerator {}

impl ConnectionIdDecoder for EmptyConnectionIdGenerator {
    fn decode_cid<'a>(&self, _: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        Some(ConnectionIdRef::from(&[]))
    }
}

impl ConnectionIdGenerator for EmptyConnectionIdGenerator {
    fn generate_cid(&mut self) -> Option<ConnectionId> {
        Some(ConnectionId::from(&[]))
    }
    fn as_decoder(&self) -> &dyn ConnectionIdDecoder {
        self
    }
    fn generates_empty_cids(&self) -> bool {
        true
    }
}

/// An `RandomConnectionIdGenerator` produces connection IDs of
/// a fixed length and random content.  No effort is made to
/// prevent collisions.
pub struct RandomConnectionIdGenerator {
    len: usize,
}

impl RandomConnectionIdGenerator {
    #[must_use]
    pub const fn new(len: usize) -> Self {
        Self { len }
    }
}

impl ConnectionIdDecoder for RandomConnectionIdGenerator {
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        dec.decode(self.len).map(ConnectionIdRef::from)
    }
}

impl ConnectionIdGenerator for RandomConnectionIdGenerator {
    fn generate_cid(&mut self) -> Option<ConnectionId> {
        let mut buf = smallvec![0; self.len];
        randomize(&mut buf);
        Some(ConnectionId::from(buf))
    }

    fn as_decoder(&self) -> &dyn ConnectionIdDecoder {
        self
    }

    fn generates_empty_cids(&self) -> bool {
        self.len == 0
    }
}

/// A single connection ID, as saved from `NEW_CONNECTION_ID`.
/// This is templated so that the connection ID entries from a peer can be
/// saved with a stateless reset token.  Local entries don't need that.
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ConnectionIdEntry<SRT: Clone + PartialEq> {
    /// The sequence number.
    seqno: u64,
    /// The connection ID.
    cid: ConnectionId,
    /// The corresponding stateless reset token.
    srt: SRT,
}

impl ConnectionIdEntry<[u8; 16]> {
    /// Create a random stateless reset token so that it is hard to guess the correct
    /// value and reset the connection.
    pub fn random_srt() -> [u8; 16] {
        random::<16>()
    }

    /// Create the first entry, which won't have a stateless reset token.
    pub fn initial_remote(cid: ConnectionId) -> Self {
        Self::new(CONNECTION_ID_SEQNO_INITIAL, cid, Self::random_srt())
    }

    /// Create an empty for when the peer chooses empty connection IDs.
    /// This uses a special sequence number just because it can.
    pub fn empty_remote() -> Self {
        Self::new(
            CONNECTION_ID_SEQNO_EMPTY,
            ConnectionId::from(&[]),
            Self::random_srt(),
        )
    }

    fn token_equal(a: &[u8; 16], b: &[u8; 16]) -> bool {
        // rustc might decide to optimize this and make this non-constant-time
        // with respect to `t`, but it doesn't appear to currently.
        let mut c = 0;
        for (&a, &b) in a.iter().zip(b) {
            c |= a ^ b;
        }
        c == 0
    }

    /// Determine whether this is a valid stateless reset.
    pub fn is_stateless_reset(&self, token: &[u8; 16]) -> bool {
        // A sequence number of 2^62 or more has no corresponding stateless reset token.
        (self.seqno < (1 << 62)) && Self::token_equal(&self.srt, token)
    }

    /// Return true if the two contain any equal parts.
    fn any_part_equal(&self, other: &Self) -> bool {
        self.seqno == other.seqno || self.cid == other.cid || self.srt == other.srt
    }

    /// The sequence number of this entry.
    pub const fn sequence_number(&self) -> u64 {
        self.seqno
    }

    /// Write the entry out in a `NEW_CONNECTION_ID` frame.
    /// Returns `true` if the frame was written, `false` if there is insufficient space.
    pub fn write(&self, builder: &mut PacketBuilder, stats: &mut FrameStats) -> bool {
        let len = 1 + Encoder::varint_len(self.seqno) + 1 + 1 + self.cid.len() + 16;
        if builder.remaining() < len {
            return false;
        }

        builder.encode_varint(FrameType::NewConnectionId);
        builder.encode_varint(self.seqno);
        builder.encode_varint(0u64);
        builder.encode_vec(1, &self.cid);
        builder.encode(&self.srt);
        stats.new_connection_id += 1;
        true
    }

    pub fn is_empty(&self) -> bool {
        self.seqno == CONNECTION_ID_SEQNO_EMPTY || self.cid.is_empty()
    }
}

impl ConnectionIdEntry<()> {
    /// Create an initial entry.
    pub const fn initial_local(cid: ConnectionId) -> Self {
        Self::new(0, cid, ())
    }
}

impl<SRT: Clone + PartialEq> ConnectionIdEntry<SRT> {
    pub const fn new(seqno: u64, cid: ConnectionId, srt: SRT) -> Self {
        Self { seqno, cid, srt }
    }

    /// Update the stateless reset token.  This panics if the sequence number is non-zero.
    pub fn set_stateless_reset_token(&mut self, srt: SRT) {
        assert_eq!(self.seqno, CONNECTION_ID_SEQNO_INITIAL);
        self.srt = srt;
    }

    /// Replace the connection ID.  This panics if the sequence number is non-zero.
    pub fn update_cid(&mut self, cid: ConnectionId) {
        assert_eq!(self.seqno, CONNECTION_ID_SEQNO_INITIAL);
        self.cid = cid;
    }

    pub const fn connection_id(&self) -> &ConnectionId {
        &self.cid
    }

    pub const fn reset_token(&self) -> &SRT {
        &self.srt
    }
}

pub type RemoteConnectionIdEntry = ConnectionIdEntry<[u8; 16]>;

/// A collection of connection IDs that are indexed by a sequence number.
/// Used to store connection IDs that are provided by a peer.
#[derive(Debug, Default)]
pub struct ConnectionIdStore<SRT: Clone + PartialEq> {
    cids: SmallVec<[ConnectionIdEntry<SRT>; 8]>,
}

impl<SRT: Clone + PartialEq> ConnectionIdStore<SRT> {
    pub fn retire(&mut self, seqno: u64) {
        self.cids.retain(|c| c.seqno != seqno);
    }

    pub fn contains(&self, cid: ConnectionIdRef) -> bool {
        self.cids.iter().any(|c| c.cid == cid)
    }

    pub fn next(&mut self) -> Option<ConnectionIdEntry<SRT>> {
        if self.cids.is_empty() {
            None
        } else {
            Some(self.cids.remove(0))
        }
    }

    pub fn len(&self) -> usize {
        self.cids.len()
    }
}

impl ConnectionIdStore<[u8; 16]> {
    pub fn add_remote(&mut self, entry: ConnectionIdEntry<[u8; 16]>) -> Res<()> {
        // It's OK if this perfectly matches an existing entry.
        if self.cids.iter().any(|c| c == &entry) {
            return Ok(());
        }
        // It's not OK if any individual piece matches though.
        if self.cids.iter().any(|c| c.any_part_equal(&entry)) {
            qinfo!("ConnectionIdStore found reused part in NEW_CONNECTION_ID");
            return Err(Error::ProtocolViolation);
        }

        // Insert in order so that we use them in order where possible.
        if let Err(idx) = self.cids.binary_search_by_key(&entry.seqno, |e| e.seqno) {
            self.cids.insert(idx, entry);
            Ok(())
        } else {
            Err(Error::ProtocolViolation)
        }
    }

    // Retire connection IDs and return the sequence numbers of those that were retired.
    pub fn retire_prior_to(&mut self, retire_prior: u64) -> Vec<u64> {
        let mut retired = Vec::new();
        self.cids.retain(|e| {
            if !e.is_empty() && e.seqno < retire_prior {
                retired.push(e.seqno);
                false
            } else {
                true
            }
        });
        retired
    }
}

impl ConnectionIdStore<()> {
    fn add_local(&mut self, entry: ConnectionIdEntry<()>) {
        self.cids.push(entry);
    }
}

pub struct ConnectionIdDecoderRef<'a> {
    generator: Ref<'a, dyn ConnectionIdGenerator>,
}

// Ideally this would be an implementation of `Deref`, but it doesn't
// seem to be possible to convince the compiler to build anything useful.
impl<'a: 'b, 'b> ConnectionIdDecoderRef<'a> {
    pub fn as_ref(&'a self) -> &'b dyn ConnectionIdDecoder {
        self.generator.as_decoder()
    }
}

/// A connection ID manager looks after the generation of connection IDs,
/// the set of connection IDs that are valid for the connection, and the
/// generation of `NEW_CONNECTION_ID` frames.
pub struct ConnectionIdManager {
    /// The `ConnectionIdGenerator` instance that is used to create connection IDs.
    generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
    /// The connection IDs that we will accept.
    /// This includes any we advertise in `NEW_CONNECTION_ID` that haven't been bound to a path
    /// yet. During the handshake at the server, it also includes the randomized DCID pick by
    /// the client.
    connection_ids: ConnectionIdStore<()>,
    /// The maximum number of connection IDs this will accept.  This is at least 2 and won't
    /// be more than `LOCAL_ACTIVE_CID_LIMIT`.
    limit: usize,
    /// The next sequence number that will be used for sending `NEW_CONNECTION_ID` frames.
    next_seqno: u64,
    /// Outstanding, but lost `NEW_CONNECTION_ID` frames will be stored here.
    lost_new_connection_id: Vec<ConnectionIdEntry<[u8; 16]>>,
}

impl ConnectionIdManager {
    pub fn new(generator: Rc<RefCell<dyn ConnectionIdGenerator>>, initial: ConnectionId) -> Self {
        let mut connection_ids = ConnectionIdStore::default();
        connection_ids.add_local(ConnectionIdEntry::initial_local(initial));
        Self {
            generator,
            connection_ids,
            // A note about initializing the limit to 2.
            // For a server, the number of connection IDs that are tracked at the point that
            // it is first possible to send `NEW_CONNECTION_ID` is 2.  One is the client-generated
            // destination connection (stored with a sequence number of `HANDSHAKE_SEQNO`); the
            // other being the handshake value (seqno 0).  As a result, `NEW_CONNECTION_ID`
            // won't be sent until until after the handshake completes, because this initial
            // value remains until the connection completes and transport parameters are handled.
            limit: 2,
            next_seqno: 1,
            lost_new_connection_id: Vec::new(),
        }
    }

    pub fn generator(&self) -> Rc<RefCell<dyn ConnectionIdGenerator>> {
        Rc::clone(&self.generator)
    }

    pub fn decoder(&self) -> ConnectionIdDecoderRef {
        ConnectionIdDecoderRef {
            generator: self.generator.deref().borrow(),
        }
    }

    /// Generate a connection ID and stateless reset token for a preferred address.
    pub fn preferred_address_cid(&mut self) -> Res<(ConnectionId, [u8; 16])> {
        if self.generator.deref().borrow().generates_empty_cids() {
            return Err(Error::ConnectionIdsExhausted);
        }
        if let Some(cid) = self.generator.borrow_mut().generate_cid() {
            assert_ne!(cid.len(), 0);
            debug_assert_eq!(self.next_seqno, CONNECTION_ID_SEQNO_PREFERRED);
            self.connection_ids
                .add_local(ConnectionIdEntry::new(self.next_seqno, cid.clone(), ()));
            self.next_seqno += 1;

            let srt = ConnectionIdEntry::random_srt();
            Ok((cid, srt))
        } else {
            Err(Error::ConnectionIdsExhausted)
        }
    }

    pub fn is_valid(&self, cid: ConnectionIdRef) -> bool {
        self.connection_ids.contains(cid)
    }

    pub fn retire(&mut self, seqno: u64) {
        // TODO(mt) - consider keeping connection IDs around for a short while.

        let empty_cid = seqno == CONNECTION_ID_SEQNO_EMPTY
            || self
                .connection_ids
                .cids
                .iter()
                .any(|c| c.seqno == seqno && c.cid.is_empty());
        if empty_cid {
            qdebug!("Connection ID {seqno} is zero-length, not retiring");
        } else {
            self.connection_ids.retire(seqno);
            self.lost_new_connection_id.retain(|cid| cid.seqno != seqno);
        }
    }

    /// During the handshake, a server needs to regard the client's choice of destination
    /// connection ID as valid.  This function saves it in the store in a special place.
    /// Note that this is only done *after* an Initial packet from the client is
    /// successfully processed.
    pub fn add_odcid(&mut self, cid: ConnectionId) {
        let entry = ConnectionIdEntry::new(CONNECTION_ID_SEQNO_ODCID, cid, ());
        self.connection_ids.add_local(entry);
    }

    /// Stop treating the original destination connection ID as valid.
    pub fn remove_odcid(&mut self) {
        self.connection_ids.retire(CONNECTION_ID_SEQNO_ODCID);
    }

    pub fn set_limit(&mut self, limit: u64) {
        debug_assert!(limit >= 2);
        self.limit = min(
            LOCAL_ACTIVE_CID_LIMIT,
            usize::try_from(limit).unwrap_or(LOCAL_ACTIVE_CID_LIMIT),
        );
    }

    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        if self.generator.deref().borrow().generates_empty_cids() {
            debug_assert_eq!(
                self.generator
                    .borrow_mut()
                    .generate_cid()
                    .expect("OK in debug assert")
                    .len(),
                0
            );
            return;
        }

        while let Some(entry) = self.lost_new_connection_id.pop() {
            if entry.write(builder, stats) {
                tokens.push(RecoveryToken::NewConnectionId(entry));
            } else {
                // This shouldn't happen often.
                self.lost_new_connection_id.push(entry);
                break;
            }
        }

        // Keep writing while we have fewer than the limit of active connection IDs
        // and while there is room for more.  This uses the longest connection ID
        // length to simplify (assuming Retire Prior To is just 1 byte).
        while self.connection_ids.len() < self.limit && builder.remaining() >= 47 {
            let maybe_cid = self.generator.borrow_mut().generate_cid();
            if let Some(cid) = maybe_cid {
                assert_ne!(cid.len(), 0);
                // TODO: generate the stateless reset tokens from the connection ID and a key.
                let srt = ConnectionIdEntry::random_srt();

                let seqno = self.next_seqno;
                self.next_seqno += 1;
                self.connection_ids
                    .add_local(ConnectionIdEntry::new(seqno, cid.clone(), ()));

                let entry = ConnectionIdEntry::new(seqno, cid, srt);
                entry.write(builder, stats);
                tokens.push(RecoveryToken::NewConnectionId(entry));
            }
        }
    }

    pub fn lost(&mut self, entry: &ConnectionIdEntry<[u8; 16]>) {
        self.lost_new_connection_id.push(entry.clone());
    }

    pub fn acked(&mut self, entry: &ConnectionIdEntry<[u8; 16]>) {
        self.lost_new_connection_id
            .retain(|e| e.seqno != entry.seqno);
    }
}

#[cfg(test)]
mod tests {
    use test_fixture::fixture_init;

    use crate::{cid::MAX_CONNECTION_ID_LEN, ConnectionId};

    #[test]
    fn generate_initial_cid() {
        fixture_init();
        for _ in 0..100 {
            let cid = ConnectionId::generate_initial();
            assert!(
                matches!(cid.len(), 8..=MAX_CONNECTION_ID_LEN),
                "connection ID length {cid:?}",
            );
        }
    }
}
