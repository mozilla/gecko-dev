//! Circular, a stream abstraction designed for use with nom
//!
//! Circular provides a `Buffer` type that wraps a `Vec<u8>` with a position
//! and end. Compared to a stream abstraction that would use `std::io::Read`,
//! it separates the reading and consuming phases. `Read` is designed to write
//! the data in a mutable slice and consume it from the stream as it does that.
//!
//! When used in streaming mode, nom will try to parse a slice, then tell you
//! how much it consumed. So you don't know how much data was actually used
//! until the parser returns. `Circular::Buffer` exposes a `data()` method
//! that gives an immutable slice of all the currently readable data,
//! and a `consume()` method to advance the position in the stream.
//! The `space()` and `fill()` methods are the write counterparts to those methods.
//!
//! ```
//! extern crate circular;
//!
//! use circular::Buffer;
//! use std::io::Write;
//!
//! fn main() {
//!
//!   // allocate a new Buffer
//!   let mut b = Buffer::with_capacity(10);
//!   assert_eq!(b.available_data(), 0);
//!   assert_eq!(b.available_space(), 10);
//!
//!   let res = b.write(&b"abcd"[..]);
//!   assert_eq!(res.ok(), Some(4));
//!   assert_eq!(b.available_data(), 4);
//!   assert_eq!(b.available_space(), 6);
//!
//!   //the 4 bytes we wrote are immediately available and usable for parsing
//!   assert_eq!(b.data(), &b"abcd"[..]);
//!
//!   // this will advance the position from 0 to 2. it does not modify the underlying Vec
//!   b.consume(2);
//!   assert_eq!(b.available_data(), 2);
//!   assert_eq!(b.available_space(), 6);
//!   assert_eq!(b.data(), &b"cd"[..]);
//!
//!   // shift moves the available data at the beginning of the buffer.
//!   // the position is now 0
//!   b.shift();
//!   assert_eq!(b.available_data(), 2);
//!   assert_eq!(b.available_space(), 8);
//!   assert_eq!(b.data(), &b"cd"[..]);
//! }
//!
use std::{cmp, ptr};
use std::io::{self,Write,Read};
use std::iter::repeat;

/// the Buffer contains the underlying memory and data positions
///
/// In all cases, `0 ≤ position ≤ end ≤ capacity` should be true
#[derive(Debug,PartialEq,Clone)]
pub struct Buffer {
  /// the Vec containing the data
  memory:   Vec<u8>,
  /// the current capacity of the Buffer
  capacity: usize,
  /// the current beginning of the available data
  position: usize,
  /// the current end of the available data
  /// and beginning of the available space
  end:      usize
}

impl Buffer {
  /// allocates a new buffer of maximum size `capacity`
  pub fn with_capacity(capacity: usize) -> Buffer {
    let mut v = Vec::with_capacity(capacity);
    v.extend(repeat(0).take(capacity));
    Buffer {
      memory:   v,
      capacity: capacity,
      position: 0,
      end:      0
    }
  }

  /// allocates a new buffer containing the slice `data`
  ///
  /// the buffer starts full, its available data size is exactly `data.len()`
  pub fn from_slice(data: &[u8]) -> Buffer {
    Buffer {
      memory:   Vec::from(data),
      capacity: data.len(),
      position: 0,
      end:      data.len()
    }
  }

  /// increases the size of the buffer
  ///
  /// this does nothing if the buffer is already large enough
  pub fn grow(&mut self, new_size: usize) -> bool {
    if self.capacity >= new_size {
      return false;
    }

    self.memory.resize(new_size, 0);
    self.capacity = new_size;
    true
  }

  /// returns how much data can be read from the buffer
  pub fn available_data(&self) -> usize {
    self.end - self.position
  }

  /// returns how much free space is available to write to
  pub fn available_space(&self) -> usize {
    self.capacity - self.end
  }

  /// returns the underlying vector's size
  pub fn capacity(&self) -> usize {
    self.capacity
  }

  /// returns true if there is no more data to read
  pub fn empty(&self) -> bool {
    self.position == self.end
  }

  /// advances the position tracker
  ///
  /// if the position gets past the buffer's half,
  /// this will call `shift()` to move the remaining data
  /// to the beginning of the buffer
  pub fn consume(&mut self, count: usize) -> usize {
    let cnt        = cmp::min(count, self.available_data());
    self.position += cnt;
    if self.position > self.capacity / 2 {
      //trace!("consume shift: pos {}, end {}", self.position, self.end);
      self.shift();
    }
    cnt
  }

  /// advances the position tracker
  ///
  /// This method is similar to `consume()` but will not move data
  /// to the beginning of the buffer
  pub fn consume_noshift(&mut self, count: usize) -> usize {
    let cnt        = cmp::min(count, self.available_data());
    self.position += cnt;
    cnt
  }

  /// after having written data to the buffer, use this function
  /// to indicate how many bytes were written
  ///
  /// if there is not enough available space, this function can call
  /// `shift()` to move the remaining data to the beginning of the
  /// buffer
  pub fn fill(&mut self, count: usize) -> usize {
    let cnt   = cmp::min(count, self.available_space());
    self.end += cnt;
    if self.available_space() < self.available_data() + cnt {
      //trace!("fill shift: pos {}, end {}", self.position, self.end);
      self.shift();
    }

    cnt
  }

  /// Get the current position
  ///
  /// # Examples
  /// ```
  /// use circular::Buffer;
  /// use std::io::{Read,Write};
  ///
  /// let mut output = [0;5];
  ///
  /// let mut b = Buffer::with_capacity(10);
  ///
  /// let res = b.write(&b"abcdefgh"[..]);
  ///
  /// b.read(&mut output);
  ///
  /// // Position must be 5
  /// assert_eq!(b.position(), 5);
  /// assert_eq!(b.available_data(), 3);
  /// ```
  pub fn position(&self) -> usize {
      self.position
  }

  /// moves the position and end trackers to the beginning
  /// this function does not modify the data
  pub fn reset(&mut self) {
    self.position = 0;
    self.end      = 0;
  }

  /// returns a slice with all the available data
  pub fn data(&self) -> &[u8] {
    &self.memory[self.position..self.end]
  }

  /// returns a mutable slice with all the available space to
  /// write to
  pub fn space(&mut self) -> &mut[u8] {
    &mut self.memory[self.end..self.capacity]
  }

  /// moves the data at the beginning of the buffer
  ///
  /// if the position was more than 0, it is now 0
  pub fn shift(&mut self) {
    if self.position > 0 {
      unsafe {
        let length = self.end - self.position;
        ptr::copy( (&self.memory[self.position..self.end]).as_ptr(), (&mut self.memory[..length]).as_mut_ptr(), length);
        self.position = 0;
        self.end      = length;
      }
    }
  }

  //FIXME: this should probably be rewritten, and tested extensively
  #[doc(hidden)]
  pub fn delete_slice(&mut self, start: usize, length: usize) -> Option<usize> {
    if start + length >= self.available_data() {
      return None
    }

    unsafe {
      let begin    = self.position + start;
      let next_end = self.end - length;
      ptr::copy(
        (&self.memory[begin+length..self.end]).as_ptr(),
        (&mut self.memory[begin..next_end]).as_mut_ptr(),
        self.end - (begin+length)
      );
      self.end = next_end;
    }
    Some(self.available_data())
  }

  //FIXME: this should probably be rewritten, and tested extensively
  #[doc(hidden)]
  pub fn replace_slice(&mut self, data: &[u8], start: usize, length: usize) -> Option<usize> {
    let data_len = data.len();
    if start + length > self.available_data() ||
      self.position + start + data_len > self.capacity {
      return None
    }

    unsafe {
      let begin     = self.position + start;
      let slice_end = begin + data_len;
      // we reduced the data size
      if data_len < length {
        ptr::copy(data.as_ptr(), (&mut self.memory[begin..slice_end]).as_mut_ptr(), data_len);

        ptr::copy((&self.memory[start+length..self.end]).as_ptr(), (&mut self.memory[slice_end..]).as_mut_ptr(), self.end - (start + length));
        self.end = self.end - (length - data_len);

      // we put more data in the buffer
      } else {
        ptr::copy((&self.memory[start+length..self.end]).as_ptr(), (&mut self.memory[start+data_len..]).as_mut_ptr(), self.end - (start + length));
        ptr::copy(data.as_ptr(), (&mut self.memory[begin..slice_end]).as_mut_ptr(), data_len);
        self.end = self.end + data_len - length;
      }
    }
    Some(self.available_data())
  }

  //FIXME: this should probably be rewritten, and tested extensively
  #[doc(hidden)]
  pub fn insert_slice(&mut self, data: &[u8], start: usize) -> Option<usize> {
    let data_len = data.len();
    if start > self.available_data() ||
      self.position + self.end + data_len > self.capacity {
      return None
    }

    unsafe {
      let begin     = self.position + start;
      let slice_end = begin + data_len;
      ptr::copy((&self.memory[start..self.end]).as_ptr(), (&mut self.memory[start+data_len..]).as_mut_ptr(), self.end - start);
      ptr::copy(data.as_ptr(), (&mut self.memory[begin..slice_end]).as_mut_ptr(), data_len);
      self.end = self.end + data_len;
    }
    Some(self.available_data())
  }
}

impl Write for Buffer {
  fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
    match self.space().write(buf) {
      Ok(size) => { self.fill(size); Ok(size) },
      err      => err
    }
  }

  fn flush(&mut self) -> io::Result<()> {
    Ok(())
  }
}

impl Read for Buffer {
  fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
    let len = cmp::min(self.available_data(), buf.len());
    unsafe {
      ptr::copy((&self.memory[self.position..self.position+len]).as_ptr(), buf.as_mut_ptr(), len);
      self.position += len;
    }
    Ok(len)
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  use std::io::Write;

  #[test]
  fn fill_and_consume() {
    let mut b = Buffer::with_capacity(10);
    assert_eq!(b.available_data(), 0);
    assert_eq!(b.available_space(), 10);
    let res = b.write(&b"abcd"[..]);
    assert_eq!(res.ok(), Some(4));
    assert_eq!(b.available_data(), 4);
    assert_eq!(b.available_space(), 6);

    assert_eq!(b.data(), &b"abcd"[..]);

    b.consume(2);
    assert_eq!(b.available_data(), 2);
    assert_eq!(b.available_space(), 6);
    assert_eq!(b.data(), &b"cd"[..]);

    b.shift();
    assert_eq!(b.available_data(), 2);
    assert_eq!(b.available_space(), 8);
    assert_eq!(b.data(), &b"cd"[..]);

    assert_eq!(b.write(&b"efghijklmnop"[..]).ok(), Some(8));
    assert_eq!(b.available_data(), 10);
    assert_eq!(b.available_space(), 0);
    assert_eq!(b.data(), &b"cdefghijkl"[..]);
    b.shift();
    assert_eq!(b.available_data(), 10);
    assert_eq!(b.available_space(), 0);
    assert_eq!(b.data(), &b"cdefghijkl"[..]);
  }

  #[test]
  fn delete() {
    let mut b = Buffer::with_capacity(10);
    let _ = b.write(&b"abcdefgh"[..]);
    assert_eq!(b.available_data(), 8);
    assert_eq!(b.available_space(), 2);

    assert_eq!(b.delete_slice(2, 3), Some(5));
    assert_eq!(b.available_data(), 5);
    assert_eq!(b.available_space(), 5);
    assert_eq!(b.data(), &b"abfgh"[..]);

    assert_eq!(b.delete_slice(5, 2), None);
    assert_eq!(b.delete_slice(4, 2), None);
  }

  #[test]
  fn replace() {
    let mut b = Buffer::with_capacity(10);
    let _ = b.write(&b"abcdefgh"[..]);
    assert_eq!(b.available_data(), 8);
    assert_eq!(b.available_space(), 2);

    assert_eq!(b.replace_slice(&b"ABC"[..], 2, 3), Some(8));
    assert_eq!(b.available_data(), 8);
    assert_eq!(b.available_space(), 2);
    assert_eq!(b.data(), &b"abABCfgh"[..]);

    assert_eq!(b.replace_slice(&b"XYZ"[..], 8, 3), None);
    assert_eq!(b.replace_slice(&b"XYZ"[..], 6, 3), None);

    assert_eq!(b.replace_slice(&b"XYZ"[..], 2, 4), Some(7));
    assert_eq!(b.available_data(), 7);
    assert_eq!(b.available_space(), 3);
    assert_eq!(b.data(), &b"abXYZgh"[..]);

    assert_eq!(b.replace_slice(&b"123"[..], 2, 2), Some(8));
    assert_eq!(b.available_data(), 8);
    assert_eq!(b.available_space(), 2);
    assert_eq!(b.data(), &b"ab123Zgh"[..]);
  }

  use std::str;
  #[test]
  fn set_position() {
    let mut output = [0;5];
    let mut b = Buffer::with_capacity(10);
    let _ = b.write(&b"abcdefgh"[..]);
    let _ = b.read(&mut output);
    assert_eq!(b.available_data(), 3);
    println!("{:?}", b.position());
  }

  #[test]
  fn consume_without_shift() {
    let mut b = Buffer::with_capacity(10);
    let _ = b.write(&b"abcdefgh"[..]);
    b.consume_noshift(6);
    assert_eq!(b.position(), 6);
  }
}
