use zerocopy::{FromBytes, Ref, Unaligned};

pub trait Reader {
    fn read_at<T: FromBytes + Unaligned>(&self, offset: u64) -> Option<&T>;
    fn read_slice_at<T: FromBytes + Unaligned>(&self, offset: u64, len: usize) -> Option<&[T]>;
}

impl Reader for [u8] {
    fn read_at<T: FromBytes + Unaligned>(&self, offset: u64) -> Option<&T> {
        let offset: usize = offset.try_into().ok()?;
        let end: usize = offset.checked_add(core::mem::size_of::<T>())?;
        let lv = Ref::<&[u8], T>::new_unaligned(self.get(offset..end)?)?;
        Some(lv.into_ref())
    }

    fn read_slice_at<T: FromBytes + Unaligned>(&self, offset: u64, len: usize) -> Option<&[T]> {
        let offset: usize = offset.try_into().ok()?;
        let end: usize = offset.checked_add(core::mem::size_of::<T>().checked_mul(len)?)?;
        let lv = Ref::<&[u8], [T]>::new_slice_unaligned(self.get(offset..end)?)?;
        Some(lv.into_slice())
    }
}
