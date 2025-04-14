// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_transport::ConnectionParameters;
use test_fixture::{default_client, default_server, new_client, now};

#[test]
fn sni_no_slicing_at_nonzero_offset() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    // This packet will have two CRPYTO frames [y..end] and [0..x], where x < y.
    let ch1 = client.process_output(now).dgram();
    assert_eq!(client.stats().frame_tx.crypto, 2);
    // This packet will have one CRYPTO frame [x..y].
    let _ch2 = client.process_output(now).dgram();
    assert_eq!(client.stats().frame_tx.crypto, 3);
    // We are dropping the second packet and only deliver the first.
    let ack = server.process(ch1, now).dgram();
    // Client will now RTX the second packet.
    now += client.process(ack, now).callback();
    // Make sure it only has one CRYPTO frame.
    _ = client.process_output(now).dgram();
    assert_eq!(client.stats().frame_tx.crypto, 4);
}

#[test]
fn sni_no_slicing_at_nonzero_offset_no_mlkem() {
    let mut client = new_client(ConnectionParameters::default().mlkem(false));
    let mut now = now();

    // This packet will have two CRPYTO frames [x..end] and [0..x].
    _ = client.process_output(now).dgram();
    assert_eq!(client.stats().frame_tx.crypto, 2);
    // Client will now RTX the packet.
    now += client.process_output(now).callback();
    // Make sure it has two CRYPTO frames.
    _ = client.process_output(now).dgram();
    assert_eq!(client.stats().frame_tx.crypto, 4);
}
