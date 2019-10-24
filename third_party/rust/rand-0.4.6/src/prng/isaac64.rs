// Copyright 2013 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The ISAAC-64 random number generator.

use core::slice;
use core::iter::repeat;
use core::num::Wrapping as w;
use core::fmt;

use {Rng, SeedableRng, Rand};

#[allow(bad_style)]
type w64 = w<u64>;

const RAND_SIZE_64_LEN: usize = 8;
const RAND_SIZE_64: usize = 1 << RAND_SIZE_64_LEN;

/// A random number generator that uses ISAAC-64[1], the 64-bit
/// variant of the ISAAC algorithm.
///
/// The ISAAC algorithm is generally accepted as suitable for
/// cryptographic purposes, but this implementation has not be
/// verified as such. Prefer a generator like `OsRng` that defers to
/// the operating system for cases that need high security.
///
/// [1]: Bob Jenkins, [*ISAAC: A fast cryptographic random number
/// generator*](http://www.burtleburtle.net/bob/rand/isaacafa.html)
#[derive(Copy)]
pub struct Isaac64Rng {
    cnt: usize,
    rsl: [w64; RAND_SIZE_64],
    mem: [w64; RAND_SIZE_64],
    a: w64,
    b: w64,
    c: w64,
}

static EMPTY_64: Isaac64Rng = Isaac64Rng {
    cnt: 0,
    rsl: [w(0); RAND_SIZE_64],
    mem: [w(0); RAND_SIZE_64],
    a: w(0), b: w(0), c: w(0),
};

impl Isaac64Rng {
    /// Create a 64-bit ISAAC random number generator using the
    /// default fixed seed.
    pub fn new_unseeded() -> Isaac64Rng {
        let mut rng = EMPTY_64;
        rng.init(false);
        rng
    }

    /// Initialises `self`. If `use_rsl` is true, then use the current value
    /// of `rsl` as a seed, otherwise construct one algorithmically (not
    /// randomly).
    fn init(&mut self, use_rsl: bool) {
        macro_rules! init {
            ($var:ident) => (
                let mut $var = w(0x9e3779b97f4a7c13);
            )
        }
        init!(a); init!(b); init!(c); init!(d);
        init!(e); init!(f); init!(g); init!(h);

        macro_rules! mix {
            () => {{
                a=a-e; f=f^(h>>9);  h=h+a;
                b=b-f; g=g^(a<<9);  a=a+b;
                c=c-g; h=h^(b>>23); b=b+c;
                d=d-h; a=a^(c<<15); c=c+d;
                e=e-a; b=b^(d>>14); d=d+e;
                f=f-b; c=c^(e<<20); e=e+f;
                g=g-c; d=d^(f>>17); f=f+g;
                h=h-d; e=e^(g<<14); g=g+h;
            }}
        }

        for _ in 0..4 {
            mix!();
        }

        if use_rsl {
            macro_rules! memloop {
                ($arr:expr) => {{
                    for i in (0..RAND_SIZE_64 / 8).map(|i| i * 8) {
                        a=a+$arr[i  ]; b=b+$arr[i+1];
                        c=c+$arr[i+2]; d=d+$arr[i+3];
                        e=e+$arr[i+4]; f=f+$arr[i+5];
                        g=g+$arr[i+6]; h=h+$arr[i+7];
                        mix!();
                        self.mem[i  ]=a; self.mem[i+1]=b;
                        self.mem[i+2]=c; self.mem[i+3]=d;
                        self.mem[i+4]=e; self.mem[i+5]=f;
                        self.mem[i+6]=g; self.mem[i+7]=h;
                    }
                }}
            }

            memloop!(self.rsl);
            memloop!(self.mem);
        } else {
            for i in (0..RAND_SIZE_64 / 8).map(|i| i * 8) {
                mix!();
                self.mem[i  ]=a; self.mem[i+1]=b;
                self.mem[i+2]=c; self.mem[i+3]=d;
                self.mem[i+4]=e; self.mem[i+5]=f;
                self.mem[i+6]=g; self.mem[i+7]=h;
            }
        }

        self.isaac64();
    }

    /// Refills the output buffer (`self.rsl`)
    fn isaac64(&mut self) {
        self.c = self.c + w(1);
        // abbreviations
        let mut a = self.a;
        let mut b = self.b + self.c;
        const MIDPOINT: usize =  RAND_SIZE_64 / 2;
        const MP_VEC: [(usize, usize); 2] = [(0,MIDPOINT), (MIDPOINT, 0)];
        macro_rules! ind {
            ($x:expr) => {
                *self.mem.get_unchecked((($x >> 3usize).0 as usize) & (RAND_SIZE_64 - 1))
            }
        }

        for &(mr_offset, m2_offset) in MP_VEC.iter() {
            for base in (0..MIDPOINT / 4).map(|i| i * 4) {

                macro_rules! rngstepp {
                    ($j:expr, $shift:expr) => {{
                        let base = base + $j;
                        let mix = a ^ (a << $shift);
                        let mix = if $j == 0 {!mix} else {mix};

                        unsafe {
                            let x = *self.mem.get_unchecked(base + mr_offset);
                            a = mix + *self.mem.get_unchecked(base + m2_offset);
                            let y = ind!(x) + a + b;
                            *self.mem.get_unchecked_mut(base + mr_offset) = y;

                            b = ind!(y >> RAND_SIZE_64_LEN) + x;
                            *self.rsl.get_unchecked_mut(base + mr_offset) = b;
                        }
                    }}
                }

                macro_rules! rngstepn {
                    ($j:expr, $shift:expr) => {{
                        let base = base + $j;
                        let mix = a ^ (a >> $shift);
                        let mix = if $j == 0 {!mix} else {mix};

                        unsafe {
                            let x = *self.mem.get_unchecked(base + mr_offset);
                            a = mix + *self.mem.get_unchecked(base + m2_offset);
                            let y = ind!(x) + a + b;
                            *self.mem.get_unchecked_mut(base + mr_offset) = y;

                            b = ind!(y >> RAND_SIZE_64_LEN) + x;
                            *self.rsl.get_unchecked_mut(base + mr_offset) = b;
                        }
                    }}
                }

                rngstepp!(0, 21);
                rngstepn!(1, 5);
                rngstepp!(2, 12);
                rngstepn!(3, 33);
            }
        }

        self.a = a;
        self.b = b;
        self.cnt = RAND_SIZE_64;
    }
}

// Cannot be derived because [u32; 256] does not implement Clone
impl Clone for Isaac64Rng {
    fn clone(&self) -> Isaac64Rng {
        *self
    }
}

impl Rng for Isaac64Rng {
    #[inline]
    fn next_u32(&mut self) -> u32 {
        self.next_u64() as u32
    }

    #[inline]
    fn next_u64(&mut self) -> u64 {
        if self.cnt == 0 {
            // make some more numbers
            self.isaac64();
        }
        self.cnt -= 1;

        // See corresponding location in IsaacRng.next_u32 for
        // explanation.
        debug_assert!(self.cnt < RAND_SIZE_64);
        self.rsl[(self.cnt % RAND_SIZE_64) as usize].0
    }
}

impl<'a> SeedableRng<&'a [u64]> for Isaac64Rng {
    fn reseed(&mut self, seed: &'a [u64]) {
        // make the seed into [seed[0], seed[1], ..., seed[seed.len()
        // - 1], 0, 0, ...], to fill rng.rsl.
        let seed_iter = seed.iter().map(|&x| x).chain(repeat(0u64));

        for (rsl_elem, seed_elem) in self.rsl.iter_mut().zip(seed_iter) {
            *rsl_elem = w(seed_elem);
        }
        self.cnt = 0;
        self.a = w(0);
        self.b = w(0);
        self.c = w(0);

        self.init(true);
    }

    /// Create an ISAAC random number generator with a seed. This can
    /// be any length, although the maximum number of elements used is
    /// 256 and any more will be silently ignored. A generator
    /// constructed with a given seed will generate the same sequence
    /// of values as all other generators constructed with that seed.
    fn from_seed(seed: &'a [u64]) -> Isaac64Rng {
        let mut rng = EMPTY_64;
        rng.reseed(seed);
        rng
    }
}

impl Rand for Isaac64Rng {
    fn rand<R: Rng>(other: &mut R) -> Isaac64Rng {
        let mut ret = EMPTY_64;
        unsafe {
            let ptr = ret.rsl.as_mut_ptr() as *mut u8;

            let slice = slice::from_raw_parts_mut(ptr, RAND_SIZE_64 * 8);
            other.fill_bytes(slice);
        }
        ret.cnt = 0;
        ret.a = w(0);
        ret.b = w(0);
        ret.c = w(0);

        ret.init(true);
        return ret;
    }
}

impl fmt::Debug for Isaac64Rng {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Isaac64Rng {{}}")
    }
}

#[cfg(test)]
mod test {
    use {Rng, SeedableRng};
    use super::Isaac64Rng;

    #[test]
    fn test_rng_64_rand_seeded() {
        let s = ::test::rng().gen_iter::<u64>().take(256).collect::<Vec<u64>>();
        let mut ra: Isaac64Rng = SeedableRng::from_seed(&s[..]);
        let mut rb: Isaac64Rng = SeedableRng::from_seed(&s[..]);
        assert!(::test::iter_eq(ra.gen_ascii_chars().take(100),
                                rb.gen_ascii_chars().take(100)));
    }

    #[test]
    fn test_rng_64_seeded() {
        let seed: &[_] = &[1, 23, 456, 7890, 12345];
        let mut ra: Isaac64Rng = SeedableRng::from_seed(seed);
        let mut rb: Isaac64Rng = SeedableRng::from_seed(seed);
        assert!(::test::iter_eq(ra.gen_ascii_chars().take(100),
                                rb.gen_ascii_chars().take(100)));
    }

    #[test]
    fn test_rng_64_reseed() {
        let s = ::test::rng().gen_iter::<u64>().take(256).collect::<Vec<u64>>();
        let mut r: Isaac64Rng = SeedableRng::from_seed(&s[..]);
        let string1: String = r.gen_ascii_chars().take(100).collect();

        r.reseed(&s[..]);

        let string2: String = r.gen_ascii_chars().take(100).collect();
        assert_eq!(string1, string2);
    }

    #[test]
    fn test_rng_64_true_values() {
        let seed: &[_] = &[1, 23, 456, 7890, 12345];
        let mut ra: Isaac64Rng = SeedableRng::from_seed(seed);
        // Regression test that isaac is actually using the above vector
        let v = (0..10).map(|_| ra.next_u64()).collect::<Vec<_>>();
        assert_eq!(v,
                   vec!(547121783600835980, 14377643087320773276, 17351601304698403469,
                        1238879483818134882, 11952566807690396487, 13970131091560099343,
                        4469761996653280935, 15552757044682284409, 6860251611068737823,
                        13722198873481261842));

        let seed: &[_] = &[12345, 67890, 54321, 9876];
        let mut rb: Isaac64Rng = SeedableRng::from_seed(seed);
        // skip forward to the 10000th number
        for _ in 0..10000 { rb.next_u64(); }

        let v = (0..10).map(|_| rb.next_u64()).collect::<Vec<_>>();
        assert_eq!(v,
                   vec!(18143823860592706164, 8491801882678285927, 2699425367717515619,
                        17196852593171130876, 2606123525235546165, 15790932315217671084,
                        596345674630742204, 9947027391921273664, 11788097613744130851,
                        10391409374914919106));
    }

    #[test]
    fn test_rng_clone() {
        let seed: &[_] = &[1, 23, 456, 7890, 12345];
        let mut rng: Isaac64Rng = SeedableRng::from_seed(seed);
        let mut clone = rng.clone();
        for _ in 0..16 {
            assert_eq!(rng.next_u64(), clone.next_u64());
        }
    }
}
