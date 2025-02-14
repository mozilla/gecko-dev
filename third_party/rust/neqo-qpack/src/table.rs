// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::VecDeque;

use neqo_common::qtrace;

use crate::{
    static_table::{StaticTableEntry, HEADER_STATIC_TABLE},
    Error, Res,
};

pub const ADDITIONAL_TABLE_ENTRY_SIZE: usize = 32;

pub struct LookupResult {
    pub index: u64,
    pub static_table: bool,
    pub value_matches: bool,
}

#[derive(Debug)]
pub struct DynamicTableEntry {
    base: u64,
    name: Vec<u8>,
    value: Vec<u8>,
    /// Number of header blocks that refer this entry.
    /// This is only used by the encoder.
    refs: u64,
}

impl DynamicTableEntry {
    pub const fn can_reduce(&self, first_not_acked: u64) -> bool {
        self.refs == 0 && self.base < first_not_acked
    }

    pub fn size(&self) -> usize {
        self.name.len() + self.value.len() + ADDITIONAL_TABLE_ENTRY_SIZE
    }

    pub fn add_ref(&mut self) {
        self.refs += 1;
    }

    pub fn remove_ref(&mut self) {
        assert!(self.refs > 0);
        self.refs -= 1;
    }

    pub fn name(&self) -> &[u8] {
        &self.name
    }

    pub fn value(&self) -> &[u8] {
        &self.value
    }

    pub const fn index(&self) -> u64 {
        self.base
    }
}

#[derive(Debug)]
pub struct HeaderTable {
    dynamic: VecDeque<DynamicTableEntry>,
    /// The total capacity (in QPACK bytes) of the table. This is set by
    /// configuration.
    capacity: u64,
    /// The amount of used capacity.
    used: u64,
    /// The total number of inserts thus far.
    base: u64,
    /// This is number of inserts that are acked. this correspond to index of the first not acked.
    /// This is only used by the encoder.
    acked_inserts_cnt: u64,
}

impl ::std::fmt::Display for HeaderTable {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "HeaderTable for (base={} acked_inserts_cnt={} capacity={})",
            self.base, self.acked_inserts_cnt, self.capacity
        )
    }
}

impl HeaderTable {
    pub const fn new(encoder: bool) -> Self {
        Self {
            dynamic: VecDeque::new(),
            capacity: 0,
            used: 0,
            base: 0,
            acked_inserts_cnt: if encoder { 0 } else { u64::MAX },
        }
    }

    /// Returns number of inserts.
    pub const fn base(&self) -> u64 {
        self.base
    }

    /// Returns capacity of the dynamic table
    pub const fn capacity(&self) -> u64 {
        self.capacity
    }

    /// Change the dynamic table capacity.
    ///
    /// # Errors
    ///
    /// [`Error::ChangeCapacity`] if table capacity cannot be reduced.
    /// The table cannot be reduced if there are entries that are referred to at
    /// the moment, or whose inserts are unacked.
    pub fn set_capacity(&mut self, cap: u64) -> Res<()> {
        qtrace!("[{self}] set capacity to {cap}");
        if !self.evict_to(cap) {
            return Err(Error::ChangeCapacity);
        }
        self.capacity = cap;
        Ok(())
    }

    /// Get a static entry with `index`.
    ///
    /// # Errors
    ///
    /// `HeaderLookup` if the index does not exist in the static table.
    pub fn get_static(index: u64) -> Res<&'static StaticTableEntry> {
        let inx = usize::try_from(index).or(Err(Error::HeaderLookup))?;
        if inx > HEADER_STATIC_TABLE.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&HEADER_STATIC_TABLE[inx])
    }

    fn get_dynamic_with_abs_index(&mut self, index: u64) -> Res<&mut DynamicTableEntry> {
        if self.base <= index {
            debug_assert!(false, "This is an internal error");
            return Err(Error::HeaderLookup);
        }
        let inx = self.base - index - 1;
        let inx = usize::try_from(inx).or(Err(Error::HeaderLookup))?;
        if inx >= self.dynamic.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&mut self.dynamic[inx])
    }

    fn get_dynamic_with_relative_index(&self, index: u64) -> Res<&DynamicTableEntry> {
        let inx = usize::try_from(index).or(Err(Error::HeaderLookup))?;
        if inx >= self.dynamic.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&self.dynamic[inx])
    }

    /// Get a entry in the  dynamic table.
    ///
    /// # Errors
    ///
    /// `HeaderLookup` if entry does not exist.
    pub fn get_dynamic(&self, index: u64, base: u64, post: bool) -> Res<&DynamicTableEntry> {
        let inx = if post {
            if self.base < (base + index + 1) {
                return Err(Error::HeaderLookup);
            }
            self.base - (base + index + 1)
        } else {
            if (self.base + index) < base {
                return Err(Error::HeaderLookup);
            }
            (self.base + index) - base
        };

        self.get_dynamic_with_relative_index(inx)
    }

    /// Remove a reference to a dynamic table entry.
    pub fn remove_ref(&mut self, index: u64) {
        qtrace!("[{self}] remove reference to entry {index}");
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .remove_ref();
    }

    /// Add a reference to a dynamic table entry.
    pub fn add_ref(&mut self, index: u64) {
        qtrace!("[{self}] add reference to entry {index}");
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .add_ref();
    }

    /// Look for a header pair.
    /// The function returns `LookupResult`: `index`, `static_table` (if it is a static table entry)
    /// and `value_matches` (if the header value matches as well not only header name)
    pub fn lookup(&mut self, name: &[u8], value: &[u8], can_block: bool) -> Option<LookupResult> {
        qtrace!("[{self}] lookup name:{name:?} value {value:?} can_block={can_block}",);
        let mut name_match = None;
        for iter in HEADER_STATIC_TABLE {
            if iter.name() == name {
                if iter.value() == value {
                    return Some(LookupResult {
                        index: iter.index(),
                        static_table: true,
                        value_matches: true,
                    });
                }

                if name_match.is_none() {
                    name_match = Some(LookupResult {
                        index: iter.index(),
                        static_table: true,
                        value_matches: false,
                    });
                }
            }
        }

        for iter in &mut self.dynamic {
            if !can_block && iter.index() >= self.acked_inserts_cnt {
                continue;
            }
            if iter.name == name {
                if iter.value == value {
                    return Some(LookupResult {
                        index: iter.index(),
                        static_table: false,
                        value_matches: true,
                    });
                }

                if name_match.is_none() {
                    name_match = Some(LookupResult {
                        index: iter.index(),
                        static_table: false,
                        value_matches: false,
                    });
                }
            }
        }
        name_match
    }

    fn evict_to(&mut self, reduce: u64) -> bool {
        qtrace!(
            "[{self}] reduce table to {reduce}, currently used:{}",
            self.used,
        );
        let mut used = self.used;
        while (!self.dynamic.is_empty()) && used > reduce {
            if let Some(e) = self.dynamic.back() {
                if !e.can_reduce(self.acked_inserts_cnt) {
                    return false;
                }
                used -= u64::try_from(e.size()).unwrap();
                self.used -= u64::try_from(e.size()).unwrap();
                self.dynamic.pop_back();
            }
        }
        true
    }

    pub fn can_evict_to(&self, reduce: u64) -> bool {
        let evictable_size: usize = self
            .dynamic
            .iter()
            .rev()
            .take_while(|e| e.can_reduce(self.acked_inserts_cnt))
            .map(DynamicTableEntry::size)
            .sum();

        self.used - u64::try_from(evictable_size).unwrap() <= reduce
    }

    pub fn insert_possible(&self, size: usize) -> bool {
        u64::try_from(size).unwrap() <= self.capacity
            && self.can_evict_to(self.capacity - u64::try_from(size).unwrap())
    }

    /// Insert a new entry.
    ///
    /// # Errors
    ///
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough
    /// space and/or other entry cannot be evicted.
    pub fn insert(&mut self, name: &[u8], value: &[u8]) -> Res<u64> {
        qtrace!("[{self}] insert name={name:?} value={value:?}");
        let entry = DynamicTableEntry {
            name: name.to_vec(),
            value: value.to_vec(),
            base: self.base,
            refs: 0,
        };
        if u64::try_from(entry.size()).unwrap() > self.capacity
            || !self.evict_to(self.capacity - u64::try_from(entry.size()).unwrap())
        {
            return Err(Error::DynamicTableFull);
        }
        self.base += 1;
        self.used += u64::try_from(entry.size()).unwrap();
        let index = entry.index();
        self.dynamic.push_front(entry);
        Ok(index)
    }

    /// Insert a new entry with the name refer to by a index to static or dynamic table.
    ///
    /// # Errors
    ///
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough
    /// space and/or other entry cannot be evicted.
    /// `HeaderLookup` if the index dos not exits in the static/dynamic table.
    pub fn insert_with_name_ref(
        &mut self,
        name_static_table: bool,
        name_index: u64,
        value: &[u8],
    ) -> Res<u64> {
        qtrace!(
            "[{self}] insert with ref to index={name_index} in {} value={value:?}",
            if name_static_table {
                "static"
            } else {
                "dynamic"
            },
        );
        let name = if name_static_table {
            Self::get_static(name_index)?.name().to_vec()
        } else {
            self.get_dynamic(name_index, self.base, false)?
                .name()
                .to_vec()
        };
        self.insert(&name, value)
    }

    /// Duplicate an entry.
    ///
    /// # Errors
    ///
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough
    /// space and/or other entry cannot be evicted.
    /// `HeaderLookup` if the index dos not exits in the static/dynamic table.
    pub fn duplicate(&mut self, index: u64) -> Res<u64> {
        qtrace!("[{self}] duplicate entry={index}");
        // need to remember name and value because insert may delete the entry.
        let name: Vec<u8>;
        let value: Vec<u8>;
        {
            let entry = self.get_dynamic(index, self.base, false)?;
            name = entry.name().to_vec();
            value = entry.value().to_vec();
            qtrace!("[{self}] duplicate name={name:?} value={value:?}");
        }
        self.insert(&name, &value)
    }

    /// Increment number of acknowledge entries.
    ///
    /// # Errors
    ///
    /// `IncrementAck` if ack is greater than actual number of inserts.
    pub fn increment_acked(&mut self, increment: u64) -> Res<()> {
        qtrace!("[{self}] increment acked by {increment}");
        self.acked_inserts_cnt += increment;
        if self.base < self.acked_inserts_cnt {
            return Err(Error::IncrementAck);
        }
        Ok(())
    }

    /// Return number of acknowledge inserts.
    pub const fn get_acked_inserts_cnt(&self) -> u64 {
        self.acked_inserts_cnt
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Due to a bug in [`HeaderTable::can_evict_to`], the function would
    /// continuously subtract the size of the last entry instead of the size of
    /// each entry starting from the back.
    ///
    /// See <https://github.com/mozilla/neqo/issues/2306> for details.
    mod issue_2306 {
        use super::*;

        const VALUE: &[u8; 2] = b"42";

        /// Given two entries where the first is smaller than the second,
        /// subtracting the size of the second from the overall size twice leads
        /// to an underflow.
        #[test]
        fn can_evict_to_no_underflow() {
            let mut table = HeaderTable::new(true);
            table.set_capacity(10000).unwrap();

            table.insert(b"header1", VALUE).unwrap();
            table.insert(b"larger-header1", VALUE).unwrap();

            table.increment_acked(2).unwrap();

            assert!(table.can_evict_to(0));
        }

        /// Given two entries where only the first is acked, continuously
        /// subtracting the size of the last entry would give a false-positive
        /// on whether both entries can be evicted.
        #[test]
        fn can_evict_to_false() {
            let mut table = HeaderTable::new(true);
            table.set_capacity(10000).unwrap();

            table.insert(b"header1", VALUE).unwrap();
            table.insert(b"header2", VALUE).unwrap();

            table.increment_acked(1).unwrap();

            let first_entry_size = table.get_dynamic_with_abs_index(0).unwrap().size() as u64;

            assert!(table.can_evict_to(first_entry_size));
            assert!(!table.can_evict_to(0));
        }
    }
}
