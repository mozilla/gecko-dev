/// Add a signed integer to this unsigned integer, with wrapping.
#[allow(unused)]
pub fn wrapping_add_signed<T: AddSigned>(lhs: T, rhs: T::Signed) -> T {
    lhs.wrapping_add_signed(rhs)
}

/// Add a signed integer to this unsigned integer, but only if doing so
/// does not cause underflow / overflow.
pub fn checked_add_signed<T: AddSigned>(lhs: T, rhs: T::Signed) -> Option<T> {
    lhs.checked_add_signed(rhs)
}

/// A trait which adds method to unsigned integers which allow checked and
/// wrapping addition of the corresponding signed integer type.
/// Unfortunately, these methods conflict with the proposed standard rust
/// methods, so this trait isn't actually usable without risking build
/// errors once these methods are stabilized.
/// https://github.com/rust-lang/rust/issues/87840
pub trait AddSigned: Sized {
    type Signed;

    /// Add a signed integer to this unsigned integer, with wrapping.
    fn wrapping_add_signed(self, rhs: Self::Signed) -> Self;

    /// Add a signed integer to this unsigned integer, but only if doing so
    /// does not cause underflow / overflow.
    fn checked_add_signed(self, rhs: Self::Signed) -> Option<Self>;
}

impl AddSigned for u64 {
    type Signed = i64;

    fn wrapping_add_signed(self, rhs: i64) -> u64 {
        self.wrapping_add(rhs as u64)
    }

    fn checked_add_signed(self, rhs: i64) -> Option<u64> {
        let res = AddSigned::wrapping_add_signed(self, rhs);
        if (rhs >= 0 && res >= self) || (rhs < 0 && res < self) {
            Some(res)
        } else {
            None
        }
    }
}

impl AddSigned for u32 {
    type Signed = i32;

    fn wrapping_add_signed(self, rhs: i32) -> u32 {
        self.wrapping_add(rhs as u32)
    }

    fn checked_add_signed(self, rhs: i32) -> Option<u32> {
        let res = AddSigned::wrapping_add_signed(self, rhs);
        if (rhs >= 0 && res >= self) || (rhs < 0 && res < self) {
            Some(res)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod test {
    use super::{checked_add_signed, wrapping_add_signed};

    #[test]
    fn test_wrapping() {
        assert_eq!(wrapping_add_signed(1, 2), 3u64);
        assert_eq!(wrapping_add_signed(2, 1), 3u64);
        assert_eq!(wrapping_add_signed(5, -4), 1u64);
        assert_eq!(wrapping_add_signed(5, -5), 0u64);
        assert_eq!(wrapping_add_signed(u64::MAX - 5, 3), u64::MAX - 2);
        assert_eq!(wrapping_add_signed(u64::MAX - 5, 5), u64::MAX);
        assert_eq!(wrapping_add_signed(u64::MAX - 5, -5), u64::MAX - 10);
        assert_eq!(wrapping_add_signed(1, -2), u64::MAX);
        assert_eq!(wrapping_add_signed(2, -4), u64::MAX - 1);
        assert_eq!(wrapping_add_signed(u64::MAX, 1), 0);
        assert_eq!(wrapping_add_signed(u64::MAX - 5, 6), 0);
        assert_eq!(wrapping_add_signed(u64::MAX - 5, 9), 3);
    }

    #[test]
    fn test_checked() {
        assert_eq!(checked_add_signed(1, 2), Some(3u64));
        assert_eq!(checked_add_signed(2, 1), Some(3u64));
        assert_eq!(checked_add_signed(5, -4), Some(1u64));
        assert_eq!(checked_add_signed(5, -5), Some(0u64));
        assert_eq!(checked_add_signed(u64::MAX - 5, 3), Some(u64::MAX - 2));
        assert_eq!(checked_add_signed(u64::MAX - 5, 5), Some(u64::MAX));
        assert_eq!(checked_add_signed(u64::MAX - 5, -5), Some(u64::MAX - 10));
        assert_eq!(checked_add_signed(1u64, -2), None);
        assert_eq!(checked_add_signed(2u64, -4), None);
        assert_eq!(checked_add_signed(u64::MAX, 1), None);
        assert_eq!(checked_add_signed(u64::MAX - 5, 6), None);
        assert_eq!(checked_add_signed(u64::MAX - 5, 9), None);
    }
}
