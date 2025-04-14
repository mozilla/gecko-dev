// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{mem, time::Instant};

use neqo_common::{qdebug, qinfo, Datagram};

use crate::crypto::Epoch;

/// The number of datagrams that are saved during the handshake when
/// keys to decrypt them are not yet available.
const MAX_SAVED_DATAGRAMS: usize = 4;

pub struct SavedDatagram {
    /// The datagram.
    pub d: Datagram,
    /// The time that the datagram was received.
    pub t: Instant,
}

#[derive(Default)]
pub struct SavedDatagrams {
    handshake: Vec<SavedDatagram>,
    application_data: Vec<SavedDatagram>,
    available: Option<Epoch>,
}

impl SavedDatagrams {
    fn store(&mut self, epoch: Epoch) -> &mut Vec<SavedDatagram> {
        match epoch {
            Epoch::Handshake => &mut self.handshake,
            Epoch::ApplicationData => &mut self.application_data,
            _ => panic!("unexpected space"),
        }
    }

    pub fn save(&mut self, epoch: Epoch, d: Datagram, t: Instant) {
        let store = self.store(epoch);

        if store.len() < MAX_SAVED_DATAGRAMS {
            qdebug!("saving datagram of {} bytes", d.len());
            store.push(SavedDatagram { d, t });
        } else {
            qinfo!("not saving datagram of {} bytes", d.len());
        }
    }

    pub fn make_available(&mut self, epoch: Epoch) {
        debug_assert_ne!(epoch, Epoch::ZeroRtt);
        debug_assert_ne!(epoch, Epoch::Initial);
        if !self.store(epoch).is_empty() {
            self.available = Some(epoch);
        }
    }

    pub const fn available(&self) -> Option<Epoch> {
        self.available
    }

    pub fn take_saved(&mut self) -> Vec<SavedDatagram> {
        self.available
            .take()
            .map_or_else(Vec::new, |epoch| mem::take(self.store(epoch)))
    }
}
