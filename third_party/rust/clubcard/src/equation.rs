/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cmp::min;

/// An Equation\<W\> is a representation of a GF(2) linear functional
///     a(x) = b + sum_i a_i x_i
/// where a_i is equal to zero except for i in a block of 64*W coefficients
/// starting at i=s. We say an Equation is /aligned/ if a_s = 1.
/// (Note: a_i above denotes the i-th bit, not the i'th 64-bit limb.)
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Equation<const W: usize> {
    pub s: usize,    // the row number
    pub a: [u64; W], // the non-trivial columns
    pub b: u8,       // the constant term
}

impl<const W: usize> Equation<W> {
    /// Construct the equation a(x) = sum_{i=s}^{s+64*W} a_i x^i.
    /// The result is aligned.
    pub fn homogeneous(s: usize, a: [u64; W]) -> Equation<W> {
        Equation::inhomogeneous(s, a, 0)
    }

    /// Construct the equation a(x) = b + sum_{i=s}^{s+64*W} a_i x^i.
    /// The result is aligned.
    pub fn inhomogeneous(s: usize, a: [u64; W], b: u8) -> Equation<W> {
        let mut eq = Equation { s: 0, a, b };
        eq.add(&Equation::zero());
        eq.s += s;
        eq
    }

    /// Construct the equation a(x) = 0.
    pub fn zero() -> Self {
        Equation {
            s: 0,
            a: [0u64; W],
            b: 0,
        }
    }

    /// Is this a(x) = 1 or a(x) = 0?
    pub fn is_zero(&self) -> bool {
        // TODO: is_const? or maybe this gets the point across.
        self.a == [0u64; W]
    }

    /// Adds `other` into `self`, i.e. sets self.a ^= other.a and self.b ^= other.b and then aligns
    /// the result.
    pub fn add(&mut self, other: &Equation<W>) {
        assert!(self.s == other.s);
        // Add the equations in GF(2)
        for i in 0..W {
            self.a[i] ^= other.a[i];
        }
        self.b ^= other.b;
        // Exit early if this equation is now zero.
        if self.is_zero() {
            return;
        }
        // Shift until there is a non-zero bit in the lowest limb.
        while self.a[0] == 0 {
            self.a.rotate_left(1);
        }
        // Shift first non-zero bit to position 0.
        let k = self.a[0].trailing_zeros();
        if k == 0 {
            return;
        }
        for i in 0..W - 1 {
            self.a[i] >>= k;
            self.a[i] |= self.a[i + 1] << (64 - k);
        }
        self.a[W - 1] >>= k;
        // Update the starting position
        self.s += k as usize;
    }

    /// Computes a(z) = sum a_i z_i.
    pub fn eval(&self, z: &[u64]) -> u8 {
        // Compute a(z), noting that this only depends
        // on 64*W bits of z starting from position s.
        let limb = self.s / 64;
        let shift = self.s % 64;
        let mut r = 0;
        for i in limb..min(z.len(), limb + W) {
            let mut tmp = z[i] >> shift;
            if i + 1 < z.len() && shift != 0 {
                tmp |= z[i + 1] << (64 - shift);
            }
            r ^= tmp & self.a[i - limb];
        }
        (r.count_ones() & 1) as u8
    }
}

#[cfg(test)]
mod tests {
    use crate::Equation;

    #[test]
    fn test_equation_add() {
        let mut e1 = Equation {
            s: 127,
            a: [0b11],
            b: 1,
        };
        let e2 = Equation {
            s: 127,
            a: [0b01],
            b: 1,
        };
        e1.add(&e2);
        // test that shifting works
        assert!(e1.s == 128);
        assert!(e1.a[0] == 0b1);
        assert!(e1.b == 0);

        let mut e1 = Equation {
            s: 127,
            a: [0b11, 0b1110, 0b1, 0],
            b: 1,
        };
        let e2 = Equation {
            s: 127,
            a: [0b01, 0b0100, 0b0, 0],
            b: 1,
        };
        e1.add(&e2);
        // test that shifting works
        assert!(e1.s == 128);
        assert!(e1.a[0] == 0b1);
        // test that bits move between limbs
        assert!(e1.a[1] == (1 << 63) | 0b101);
        assert!(e1.a[2] == 0);
        assert!(e1.a[3] == 0);
        assert!(e1.b == 0);
    }

    #[test]
    fn test_equation_eval() {
        for s in 0..64 {
            let eq = Equation {
                s,
                a: [0xffffffffffffffff, 0, 0, 0],
                b: 0,
            };
            assert!(0 == eq.eval(&[]));
            for i in 0..64 {
                assert!(((i >= eq.s) as u8) == eq.eval(&[1 << i, 0]));
                assert!(((i < eq.s) as u8) == eq.eval(&[0, 1 << i]));
                assert!(0 == eq.eval(&[0, 0, 1 << i]));
            }
        }
    }
}
