// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use neqo_common::Decoder;
use neqo_crypto::{init, randomize};

fn randomize_buffer(n: usize, mask: u8) -> Vec<u8> {
    let mut buf = vec![0; n];
    // NSS doesn't like randomizing larger buffers, so chunk them up.
    // https://searchfox.org/nss/rev/968939484921b0ceecca189cd1b66e97950c39da/lib/freebl/drbg.c#29
    for chunk in buf.chunks_mut(0x10000) {
        randomize(chunk);
    }
    // Masking the top bits off causes the resulting values to be interpreted as
    // smaller varints, which stresses the decoder differently.
    // This is worth testing because most varints contain small values.
    for x in &mut buf[..] {
        *x &= mask;
    }
    buf
}

fn decoder(c: &mut Criterion, count: usize, mask: u8) {
    c.bench_function(&format!("decode {count} bytes, mask {mask:x}"), |b| {
        b.iter_batched_ref(
            || randomize_buffer(count, mask),
            |buf| {
                let mut dec = Decoder::new(&buf[..]);
                while black_box(dec.decode_varint()).is_some() {
                    // Do nothing;
                }
            },
            criterion::BatchSize::SmallInput,
        );
    });
}

fn benchmark_decoder(c: &mut Criterion) {
    init().unwrap();
    for mask in [0xff, 0x7f, 0x3f] {
        for exponent in [12, 20] {
            decoder(c, 1 << exponent, mask);
        }
    }
}

criterion_group!(benches, benchmark_decoder);
criterion_main!(benches);
