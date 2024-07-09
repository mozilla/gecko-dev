//! Trait implementations for `ArrayString` (that aren't for integration)

use crate::{generic::ArraySlice, prelude::*};
use core::fmt::{self, Debug, Display, Formatter, Write};
use core::iter::FromIterator;
use core::ops::{Add, Deref, DerefMut, Index, IndexMut};
use core::ops::{Range, RangeFrom, RangeFull, RangeInclusive, RangeTo, RangeToInclusive};
use core::str::{self, FromStr};
use core::{borrow::Borrow, borrow::BorrowMut, cmp::Ordering, hash::Hash, hash::Hasher};

impl<SIZE> Default for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn default() -> Self {
        Self {
            array: SIZE::Array::zeroed(),
            size: Default::default(),
        }
    }
}

impl<SIZE: Capacity + Copy> Copy for ArrayString<SIZE> where SIZE::Array: Copy {}

impl<SIZE> AsRef<str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn as_ref(&self) -> &str {
        unsafe { str::from_utf8_unchecked(self.as_ref()) }
    }
}

impl<SIZE> AsMut<str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn as_mut(&mut self) -> &mut str {
        let len = self.size as usize;
        let slice = unsafe { self.array.as_mut_slice().get_unchecked_mut(..len) };
        unsafe { str::from_utf8_unchecked_mut(slice) }
    }
}

impl<SIZE> AsRef<[u8]> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn as_ref(&self) -> &[u8] {
        unsafe { self.array.as_slice().get_unchecked(..self.size.into()) }
    }
}

impl<'a, SIZE> From<&'a str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn from(s: &str) -> Self {
        Self::from_str_truncate(s)
    }
}

impl<SIZE> FromStr for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Err = OutOfBounds;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(s)
    }
}

impl<SIZE> Debug for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.debug_struct("ArrayString")
            .field("array", &self.as_str())
            .field("size", &self.size)
            .finish()
    }
}

impl<'a, 'b, SIZE> PartialEq<str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.as_str().eq(other)
    }
}

impl<SIZE> Borrow<str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl<SIZE> BorrowMut<str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn borrow_mut(&mut self) -> &mut str {
        self.as_mut_str()
    }
}

impl<SIZE> Hash for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.as_str().hash(hasher);
    }
}

impl<SIZE> PartialEq for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.as_str().eq(other.as_str())
    }
}
impl<SIZE: Capacity> Eq for ArrayString<SIZE> {}

impl<SIZE> Ord for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl<SIZE> PartialOrd for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a, SIZE> Add<&'a str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = Self;

    #[inline]
    fn add(self, other: &str) -> Self::Output {
        let mut out = unsafe { Self::from_str_unchecked(self) };
        out.push_str(other);
        out
    }
}

impl<SIZE> Write for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn write_str(&mut self, slice: &str) -> fmt::Result {
        self.try_push_str(slice).map_err(|_| fmt::Error)
    }
}

impl<SIZE> Display for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl<SIZE> Deref for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Target = str;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl<SIZE> DerefMut for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut()
    }
}

impl<SIZE> FromIterator<char> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    fn from_iter<I: IntoIterator<Item = char>>(iter: I) -> Self {
        Self::from_chars(iter)
    }
}

impl<'a, SIZE> FromIterator<&'a str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    fn from_iter<I: IntoIterator<Item = &'a str>>(iter: I) -> Self {
        Self::from_iterator(iter)
    }
}

impl<SIZE> Extend<char> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    fn extend<I: IntoIterator<Item = char>>(&mut self, iterable: I) {
        self.push_str(Self::from_chars(iterable))
    }
}

impl<'a, SIZE> Extend<&'a char> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    fn extend<I: IntoIterator<Item = &'a char>>(&mut self, iter: I) {
        self.extend(iter.into_iter().cloned());
    }
}

impl<'a, SIZE> Extend<&'a str> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    fn extend<I: IntoIterator<Item = &'a str>>(&mut self, iterable: I) {
        self.push_str(Self::from_iterator(iterable))
    }
}

impl<SIZE> IndexMut<RangeFrom<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: RangeFrom<u8>) -> &mut str {
        let start = index.start as usize;
        self.as_mut_str().index_mut(RangeFrom { start })
    }
}

impl<SIZE> IndexMut<RangeTo<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: RangeTo<u8>) -> &mut str {
        let end = index.end as usize;
        self.as_mut_str().index_mut(RangeTo { end })
    }
}

impl<SIZE> IndexMut<RangeFull> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: RangeFull) -> &mut str {
        self.as_mut_str().index_mut(index)
    }
}

impl<SIZE> IndexMut<Range<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: Range<u8>) -> &mut str {
        let (start, end) = (index.start as usize, index.end as usize);
        let range = Range { start, end };
        self.as_mut_str().index_mut(range)
    }
}

impl<SIZE> IndexMut<RangeToInclusive<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: RangeToInclusive<u8>) -> &mut str {
        let end = index.end as usize;
        let range = RangeToInclusive { end };
        self.as_mut_str().index_mut(range)
    }
}

impl<SIZE> IndexMut<RangeInclusive<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn index_mut(&mut self, index: RangeInclusive<u8>) -> &mut str {
        let (start, end) = (*index.start() as usize, *index.end() as usize);
        let range = RangeInclusive::new(start, end);
        self.as_mut_str().index_mut(range)
    }
}

impl<SIZE> Index<RangeFrom<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: RangeFrom<u8>) -> &Self::Output {
        let start = index.start as usize;
        self.as_str().index(RangeFrom { start })
    }
}

impl<SIZE> Index<RangeTo<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: RangeTo<u8>) -> &Self::Output {
        let end = index.end as usize;
        self.as_str().index(RangeTo { end })
    }
}

impl<SIZE> Index<RangeFull> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: RangeFull) -> &Self::Output {
        self.as_str().index(index)
    }
}

impl<SIZE> Index<Range<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: Range<u8>) -> &Self::Output {
        let (start, end) = (index.start as usize, index.end as usize);
        self.as_str().index(Range { start, end })
    }
}

impl<SIZE> Index<RangeToInclusive<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: RangeToInclusive<u8>) -> &Self::Output {
        let end = index.end as usize;
        self.as_str().index(RangeToInclusive { end })
    }
}

impl<SIZE> Index<RangeInclusive<u8>> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    type Output = str;

    #[inline]
    fn index(&self, index: RangeInclusive<u8>) -> &Self::Output {
        let (start, end) = (*index.start() as usize, *index.end() as usize);
        let range = RangeInclusive::new(start, end);
        self.as_str().index(range)
    }
}
