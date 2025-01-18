#![no_std]
#![feature(test)]

extern crate test;

use test::Bencher;

#[bench]
fn bench_compress256(b: &mut Bencher) {
    let mut state = Default::default();
    let data = [[0u8; 64]];

    b.iter(|| {
        sha2_asm::compress256(&mut state, &data);
    });

    b.bytes = data.len() as u64;
}

#[cfg(not(target_arch = "aarch64"))]
#[bench]
fn bench_compress512(b: &mut Bencher) {
    let mut state = Default::default();
    let data = [[0u8; 128]];

    b.iter(|| {
        sha2_asm::compress512(&mut state, &data);
    });

    b.bytes = data.len() as u64;
}
