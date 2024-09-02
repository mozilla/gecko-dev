#![allow(non_snake_case)] // TODO ultimately remove this
#![allow(clippy::missing_safety_doc)] // obviously needs to be fixed long-term

use core::ffi::{c_char, c_int, c_long, c_ulong};
use core::marker::PhantomData;
use core::mem::MaybeUninit;

mod bitreader;
mod inffixed_tbl;
mod inftrees;
mod window;

use crate::allocate::Allocator;
use crate::c_api::internal_state;
use crate::{
    adler32::adler32,
    c_api::{gz_header, z_checksum, z_size, z_stream, Z_DEFLATED},
    read_buf::ReadBuf,
    Code, InflateFlush, ReturnCode, DEF_WBITS, MAX_WBITS, MIN_WBITS,
};

use crate::crc32::{crc32, Crc32Fold};

use self::{
    bitreader::BitReader,
    inftrees::{inflate_table, CodeType, InflateTable},
    window::Window,
};

#[repr(C)]
pub struct InflateStream<'a> {
    pub(crate) next_in: *mut crate::c_api::Bytef,
    pub(crate) avail_in: crate::c_api::uInt,
    pub(crate) total_in: crate::c_api::z_size,
    pub(crate) next_out: *mut crate::c_api::Bytef,
    pub(crate) avail_out: crate::c_api::uInt,
    pub(crate) total_out: crate::c_api::z_size,
    pub(crate) msg: *mut c_char,
    pub(crate) state: &'a mut State<'a>,
    pub(crate) alloc: Allocator<'a>,
    pub(crate) data_type: c_int,
    pub(crate) adler: crate::c_api::z_checksum,
    pub(crate) reserved: crate::c_api::uLong,
}

#[cfg(feature = "__internal-test")]
#[doc(hidden)]
pub const INFLATE_STATE_SIZE: usize = core::mem::size_of::<crate::inflate::State>();

#[cfg(feature = "__internal-test")]
#[doc(hidden)]
pub unsafe fn set_mode_dict(strm: &mut z_stream) {
    unsafe {
        (*(strm.state as *mut State)).mode = Mode::Dict;
    }
}

impl<'a> InflateStream<'a> {
    const _S: () = assert!(core::mem::size_of::<z_stream>() == core::mem::size_of::<Self>());
    const _A: () = assert!(core::mem::align_of::<z_stream>() == core::mem::align_of::<Self>());

    /// # Safety
    ///
    /// Behavior is undefined if any of the following conditions are violated:
    ///
    /// - `strm` satisfies the conditions of [`pointer::as_ref`]
    /// - if not `NULL`, `strm` as initialized using [`init`] or similar
    ///
    /// [`pointer::as_ref`]: https://doc.rust-lang.org/core/primitive.pointer.html#method.as_ref
    #[inline(always)]
    pub unsafe fn from_stream_ref(strm: *const z_stream) -> Option<&'a Self> {
        {
            // Safety: ptr points to a valid value of type z_stream (if non-null)
            let stream = unsafe { strm.as_ref() }?;

            if stream.zalloc.is_none() || stream.zfree.is_none() {
                return None;
            }

            if stream.state.is_null() {
                return None;
            }
        }

        // Safety: InflateStream has an equivalent layout as z_stream
        strm.cast::<InflateStream>().as_ref()
    }

    /// # Safety
    ///
    /// Behavior is undefined if any of the following conditions are violated:
    ///
    /// - `strm` satisfies the conditions of [`pointer::as_mut`]
    /// - if not `NULL`, `strm` as initialized using [`init`] or similar
    ///
    /// [`pointer::as_mut`]: https://doc.rust-lang.org/core/primitive.pointer.html#method.as_mut
    #[inline(always)]
    pub unsafe fn from_stream_mut(strm: *mut z_stream) -> Option<&'a mut Self> {
        {
            // Safety: ptr points to a valid value of type z_stream (if non-null)
            let stream = unsafe { strm.as_ref() }?;

            if stream.zalloc.is_none() || stream.zfree.is_none() {
                return None;
            }

            if stream.state.is_null() {
                return None;
            }
        }

        // Safety: InflateStream has an equivalent layout as z_stream
        strm.cast::<InflateStream>().as_mut()
    }

    fn as_z_stream_mut(&mut self) -> &mut z_stream {
        // safety: a valid &mut InflateStream is also a valid &mut z_stream
        unsafe { &mut *(self as *mut _ as *mut z_stream) }
    }
}

const MAX_BITS: u8 = 15; // maximum number of bits in a code
const MAX_DIST_EXTRA_BITS: u8 = 13; // maximum number of extra distance bits
                                    //
pub fn uncompress_slice<'a>(
    output: &'a mut [u8],
    input: &[u8],
    config: InflateConfig,
) -> (&'a mut [u8], ReturnCode) {
    let output_uninit = unsafe {
        core::slice::from_raw_parts_mut(output.as_mut_ptr() as *mut MaybeUninit<u8>, output.len())
    };

    uncompress(output_uninit, input, config)
}

/// Inflates `source` into `dest`, and writes the final inflated size into `dest_len`.
pub fn uncompress<'a>(
    output: &'a mut [MaybeUninit<u8>],
    input: &[u8],
    config: InflateConfig,
) -> (&'a mut [u8], ReturnCode) {
    let mut dest_len_ptr = output.len() as z_checksum;

    // for detection of incomplete stream when *destLen == 0
    let mut buf = [0u8];

    let mut left;
    let mut len = input.len() as u64;

    let dest = if output.is_empty() {
        left = 1;

        buf.as_mut_ptr()
    } else {
        left = output.len() as u64;
        dest_len_ptr = 0;

        output.as_mut_ptr() as *mut u8
    };

    let mut stream = z_stream {
        next_in: input.as_ptr() as *mut u8,
        avail_in: 0,

        zalloc: None,
        zfree: None,
        opaque: core::ptr::null_mut(),

        ..z_stream::default()
    };

    let err = init(&mut stream, config);
    if err != ReturnCode::Ok {
        return (&mut [], err);
    }

    stream.next_out = dest;
    stream.avail_out = 0;

    let Some(stream) = (unsafe { InflateStream::from_stream_mut(&mut stream) }) else {
        return (&mut [], ReturnCode::StreamError);
    };

    let err = loop {
        if stream.avail_out == 0 {
            stream.avail_out = Ord::min(left, u32::MAX as u64) as u32;
            left -= stream.avail_out as u64;
        }

        if stream.avail_in == 0 {
            stream.avail_in = Ord::min(len, u32::MAX as u64) as u32;
            len -= stream.avail_in as u64;
        }

        let err = unsafe { inflate(stream, InflateFlush::NoFlush) };

        if err != ReturnCode::Ok {
            break err;
        }
    };

    if !output.is_empty() {
        dest_len_ptr = stream.total_out;
    } else if stream.total_out != 0 && err == ReturnCode::BufError {
        left = 1;
    }

    let avail_out = stream.avail_out;

    end(stream);

    let ret = match err {
        ReturnCode::StreamEnd => ReturnCode::Ok,
        ReturnCode::NeedDict => ReturnCode::DataError,
        ReturnCode::BufError if (left + avail_out as u64) != 0 => ReturnCode::DataError,
        _ => err,
    };

    // SAFETY: we have now initialized these bytes
    let output_slice = unsafe {
        core::slice::from_raw_parts_mut(output.as_mut_ptr() as *mut u8, dest_len_ptr as usize)
    };

    (output_slice, ret)
}

#[derive(Debug, Clone, Copy)]
pub enum Mode {
    Head,
    Flags,
    Time,
    Os,
    ExLen,
    Extra,
    Name,
    Comment,
    HCrc,
    Sync,
    Mem,
    Length,
    Type,
    TypeDo,
    Stored,
    CopyBlock,
    Check,
    Len_,
    Len,
    Lit,
    LenExt,
    Dist,
    DistExt,
    Match,
    Table,
    LenLens,
    CodeLens,
    DictId,
    Dict,
    Done,
    Bad,
}

#[derive(Clone, Copy)]
#[allow(clippy::enum_variant_names)]
enum Codes {
    Fixed(&'static [Code]),
    Codes,
    Len,
    Dist,
}

impl Default for Codes {
    fn default() -> Self {
        Codes::Fixed(&[])
    }
}

#[derive(Default, Clone, Copy)]
struct Table {
    codes: Codes,
    bits: usize,
}

pub(crate) struct State<'a> {
    /// Current inflate mode
    mode: Mode,

    /// true if processing the last block
    last: bool,
    /// bitflag
    ///
    /// - bit 0 true if zlib
    /// - bit 1 true if gzip
    /// - bit 2 true to validate check value
    wrap: usize,

    /// table for length/literal codes
    len_table: Table,

    /// table for dist codes
    dist_table: Table,

    /// log base 2 of requested window size
    wbits: usize,
    // allocated window if needed (capacity == 0 if unused)
    window: Window<'a>,

    /// place to store gzip header if needed
    head: Option<&'a mut gz_header>,

    //
    /// number of code length code lengths
    ncode: usize,
    /// number of length code lengths
    nlen: usize,
    /// number of distance code lengths
    ndist: usize,
    /// number of code lengths in lens[]
    have: usize,
    /// next available space in codes[]
    next: usize, // represented as an index, don't want a self-referential structure here

    // IO
    bit_reader: BitReader<'a>,

    writer: ReadBuf<'a>,
    total: usize,

    /// length of a block to copy
    length: usize,
    /// distance back to copy the string from
    offset: usize,

    /// extra bits needed
    extra: usize,

    /// if false, allow invalid distance too far
    sane: bool,
    /// bits back of last unprocessed length/lit
    back: usize,

    /// initial length of match
    was: usize,

    /// size of memory copying chunk
    chunksize: usize,

    in_available: usize,
    out_available: usize,

    /// temporary storage space for code lengths
    lens: [u16; 320],
    /// work area for code table building
    work: [u16; 288],

    error_message: Option<&'static str>,
    flush: InflateFlush,

    checksum: u32,
    crc_fold: Crc32Fold,

    havedict: bool,
    dmax: usize,
    flags: i32,

    codes_codes: [Code; crate::ENOUGH_LENS],
    len_codes: [Code; crate::ENOUGH_LENS],
    dist_codes: [Code; crate::ENOUGH_DISTS],
}

impl<'a> State<'a> {
    fn new(reader: &'a [u8], writer: ReadBuf<'a>) -> Self {
        let in_available = reader.len();
        let out_available = writer.capacity();

        Self {
            flush: InflateFlush::NoFlush,

            last: false,
            wrap: 0,
            mode: Mode::Head,
            length: 0,

            len_table: Table::default(),
            dist_table: Table::default(),

            wbits: 0,
            offset: 0,
            extra: 0,
            sane: true,
            back: 0,
            was: 0,
            chunksize: 0,
            in_available,
            out_available,

            bit_reader: BitReader::new(reader),

            writer,
            total: 0,

            window: Window::empty(),
            head: None,

            lens: [0u16; 320],
            work: [0u16; 288],

            ncode: 0,
            nlen: 0,
            ndist: 0,
            have: 0,
            next: 0,

            error_message: None,

            checksum: 0,
            crc_fold: Crc32Fold::new(),

            havedict: false,
            dmax: 0,
            flags: 0,

            codes_codes: [Code::default(); crate::ENOUGH_LENS],
            len_codes: [Code::default(); crate::ENOUGH_LENS],
            dist_codes: [Code::default(); crate::ENOUGH_DISTS],
        }
    }

    fn len_table_ref(&self) -> &[Code] {
        match self.len_table.codes {
            Codes::Fixed(fixed) => fixed,
            Codes::Codes => &self.codes_codes,
            Codes::Len => &self.len_codes,
            Codes::Dist => &self.dist_codes,
        }
    }

    fn dist_table_ref(&self) -> &[Code] {
        match self.dist_table.codes {
            Codes::Fixed(fixed) => fixed,
            Codes::Codes => &self.codes_codes,
            Codes::Len => &self.len_codes,
            Codes::Dist => &self.dist_codes,
        }
    }

    fn len_table_get(&self, index: usize) -> Code {
        self.len_table_ref()[index]
    }

    fn dist_table_get(&self, index: usize) -> Code {
        self.dist_table_ref()[index]
    }
}

macro_rules! pull_byte {
    ($self:expr) => {
        match $self.bit_reader.pull_byte() {
            Err(return_code) => return $self.inflate_leave(return_code),
            Ok(_) => (),
        }
    };
}

macro_rules! need_bits {
    ($self:expr, $n:expr) => {
        match $self.bit_reader.need_bits($n) {
            Err(return_code) => return $self.inflate_leave(return_code),
            Ok(v) => v,
        }
    };
}

// swaps endianness
const fn zswap32(q: u32) -> u32 {
    u32::from_be(q.to_le())
}

const INFLATE_FAST_MIN_HAVE: usize = 15;
const INFLATE_FAST_MIN_LEFT: usize = 260;

impl<'a> State<'a> {
    fn dispatch(&mut self) -> ReturnCode {
        match self.mode {
            Mode::Head => self.head(),
            Mode::Flags => self.flags(),
            Mode::Time => self.time(),
            Mode::Os => self.os(),
            Mode::ExLen => self.ex_len(),
            Mode::Extra => self.extra(),
            Mode::Name => self.name(),
            Mode::Comment => self.comment(),
            Mode::HCrc => self.hcrc(),
            Mode::Sync => self.sync(),
            Mode::Type => self.type_(),
            Mode::TypeDo => self.type_do(),
            Mode::Stored => self.stored(),
            Mode::CopyBlock => self.copy_block(),
            Mode::Check => self.check(),
            Mode::Len => self.len(),
            Mode::Len_ => self.len_(),
            Mode::LenExt => self.len_ext(),
            Mode::Lit => self.lit(),
            Mode::Dist => self.dist(),
            Mode::DistExt => self.dist_ext(),
            Mode::Match => self.match_(),
            Mode::Done => todo!(),
            Mode::Table => self.table(),
            Mode::LenLens => self.len_lens(),
            Mode::CodeLens => self.code_lens(),
            Mode::Dict => self.dict(),
            Mode::DictId => self.dict_id(),
            Mode::Bad => self.bad("repeated call with bad state\0"),
            Mode::Mem => self.mem(),
            Mode::Length => self.length(),
        }
    }

    // ----------------

    /// Initial state
    #[inline(never)]
    fn head(&mut self) -> ReturnCode {
        if self.wrap == 0 {
            self.mode = Mode::TypeDo;
            return self.type_do();
        }

        need_bits!(self, 16);

        // Gzip
        if (self.wrap & 2) != 0 && self.bit_reader.hold() == 0x8b1f {
            if self.wbits == 0 {
                self.wbits = 15;
            }

            let b0 = self.bit_reader.bits(8) as u8;
            let b1 = (self.bit_reader.hold() >> 8) as u8;
            self.checksum = crc32(crate::CRC32_INITIAL_VALUE, &[b0, b1]);
            self.bit_reader.init_bits();

            self.mode = Mode::Flags;
            return self.flags();
        }

        // check if zlib header is allowed
        if (self.wrap & 1) != 0
            && ((self.bit_reader.bits(8) << 8) + (self.bit_reader.hold() >> 8)) % 31 != 0
        {
            self.mode = Mode::Bad;
            return self.bad("incorrect header check\0");
        }

        if self.bit_reader.bits(4) != Z_DEFLATED as u64 {
            self.mode = Mode::Bad;
            return self.bad("unknown compression method\0");
        }

        self.bit_reader.drop_bits(4);
        let len = self.bit_reader.bits(4) as usize + 8;

        if self.wbits == 0 {
            self.wbits = len;
        }

        if len > MAX_WBITS as usize || len > self.wbits {
            self.mode = Mode::Bad;
            return self.bad("invalid window size\0");
        }

        self.dmax = 1 << len;
        self.flags = 0; // indicate zlib header
        self.checksum = crate::ADLER32_INITIAL_VALUE as _;

        if self.bit_reader.hold() & 0x200 != 0 {
            self.bit_reader.init_bits();

            self.mode = Mode::DictId;
            self.dict_id()
        } else {
            self.bit_reader.init_bits();

            self.mode = Mode::Type;
            self.type_()
        }
    }

    fn flags(&mut self) -> ReturnCode {
        need_bits!(self, 16);
        self.flags = self.bit_reader.hold() as i32;

        // Z_DEFLATED = 8 is the only supported method
        if self.flags & 0xff != Z_DEFLATED {
            self.mode = Mode::Bad;
            return self.bad("unknown compression method\0");
        }

        if self.flags & 0xe000 != 0 {
            self.mode = Mode::Bad;
            return self.bad("unknown header flags set\0");
        }

        if let Some(head) = self.head.as_mut() {
            head.text = ((self.bit_reader.hold() >> 8) & 1) as i32;
        }

        if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
            let b0 = self.bit_reader.bits(8) as u8;
            let b1 = (self.bit_reader.hold() >> 8) as u8;
            self.checksum = crc32(self.checksum, &[b0, b1]);
        }

        self.bit_reader.init_bits();
        self.mode = Mode::Time;
        self.time()
    }

    fn time(&mut self) -> ReturnCode {
        need_bits!(self, 32);
        if let Some(head) = self.head.as_mut() {
            head.time = self.bit_reader.hold() as z_size;
        }

        if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
            let bytes = (self.bit_reader.hold() as u32).to_le_bytes();
            self.checksum = crc32(self.checksum, &bytes);
        }

        self.bit_reader.init_bits();
        self.mode = Mode::Os;
        self.os()
    }

    fn os(&mut self) -> ReturnCode {
        need_bits!(self, 16);
        if let Some(head) = self.head.as_mut() {
            head.xflags = (self.bit_reader.hold() & 0xff) as i32;
            head.os = (self.bit_reader.hold() >> 8) as i32;
        }

        if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
            let bytes = (self.bit_reader.hold() as u16).to_le_bytes();
            self.checksum = crc32(self.checksum, &bytes);
        }

        self.bit_reader.init_bits();
        self.mode = Mode::ExLen;
        self.ex_len()
    }

    fn ex_len(&mut self) -> ReturnCode {
        if (self.flags & 0x0400) != 0 {
            need_bits!(self, 16);

            // self.length (and head.extra_len) represent the length of the extra field
            self.length = self.bit_reader.hold() as usize;
            if let Some(head) = self.head.as_mut() {
                head.extra_len = self.length as u32;
            }

            if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
                let bytes = (self.bit_reader.hold() as u16).to_le_bytes();
                self.checksum = crc32(self.checksum, &bytes);
            }
            self.bit_reader.init_bits();
        } else if let Some(head) = self.head.as_mut() {
            head.extra = core::ptr::null_mut();
        }

        self.mode = Mode::Extra;
        self.extra()
    }

    fn extra(&mut self) -> ReturnCode {
        if (self.flags & 0x0400) != 0 {
            // self.length is the number of remaining `extra` bytes. But they may not all be available
            let extra_available = Ord::min(self.length, self.bit_reader.bytes_remaining());
            let extra_slice = &self.bit_reader.as_slice()[..extra_available];

            if !extra_slice.is_empty() {
                if let Some(head) = self.head.as_mut() {
                    if !head.extra.is_null() {
                        let written_so_far = head.extra_len as usize - self.length;

                        let count = Ord::min(
                            (head.extra_max as usize).saturating_sub(written_so_far),
                            extra_slice.len(),
                        );

                        unsafe {
                            core::ptr::copy_nonoverlapping(
                                self.bit_reader.as_ptr(),
                                head.extra.add(written_so_far),
                                count,
                            );
                        }
                    }
                }

                // Checksum
                if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
                    self.checksum = crc32(self.checksum, extra_slice)
                }

                self.in_available -= extra_available;
                self.bit_reader.advance(extra_available);
                self.length -= extra_available;
            }

            // Checks for errors occur after returning
            if self.length != 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }
        }

        self.length = 0;
        self.mode = Mode::Name;
        self.name()
    }

    fn name(&mut self) -> ReturnCode {
        if (self.flags & 0x0800) != 0 {
            if self.in_available == 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }

            // the name string will always be null-terminated, but might be longer than we have
            // space for in the header struct. Nonetheless, we read the whole thing.
            let slice = self.bit_reader.as_slice();
            let null_terminator_index = slice.iter().position(|c| *c == 0);

            // we include the null terminator if it exists
            let name_slice = match null_terminator_index {
                Some(i) => &slice[..=i],
                None => slice,
            };

            // if the header has space, store as much as possible in there
            if let Some(head) = self.head.as_mut() {
                if !head.name.is_null() {
                    let remaining_name_bytes = (head.name_max as usize).saturating_sub(self.length);
                    let copy = Ord::min(name_slice.len(), remaining_name_bytes);

                    unsafe {
                        core::ptr::copy_nonoverlapping(
                            name_slice.as_ptr(),
                            head.name.add(self.length),
                            copy,
                        )
                    };

                    self.length += copy;
                }
            }

            if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
                self.checksum = crc32(self.checksum, name_slice);
            }

            let reached_end = name_slice.last() == Some(&0);
            self.bit_reader.advance(name_slice.len());

            if !reached_end && self.bit_reader.bytes_remaining() == 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }
        } else if let Some(head) = self.head.as_mut() {
            head.name = core::ptr::null_mut();
        }

        self.length = 0;
        self.mode = Mode::Comment;
        self.comment()
    }

    fn comment(&mut self) -> ReturnCode {
        if (self.flags & 0x01000) != 0 {
            if self.in_available == 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }

            // the comment string will always be null-terminated, but might be longer than we have
            // space for in the header struct. Nonetheless, we read the whole thing.
            let slice = self.bit_reader.as_slice();
            let null_terminator_index = slice.iter().position(|c| *c == 0);

            // we include the null terminator if it exists
            let comment_slice = match null_terminator_index {
                Some(i) => &slice[..=i],
                None => slice,
            };

            // if the header has space, store as much as possible in there
            if let Some(head) = self.head.as_mut() {
                if !head.comment.is_null() {
                    let remaining_comm_bytes = (head.comm_max as usize).saturating_sub(self.length);
                    let copy = Ord::min(comment_slice.len(), remaining_comm_bytes);
                    unsafe {
                        core::ptr::copy_nonoverlapping(
                            comment_slice.as_ptr(),
                            head.comment.add(self.length),
                            copy,
                        )
                    };

                    self.length += copy;
                }
            }

            if (self.flags & 0x0200) != 0 && (self.wrap & 4) != 0 {
                self.checksum = crc32(self.checksum, comment_slice);
            }

            let reached_end = comment_slice.last() == Some(&0);
            self.bit_reader.advance(comment_slice.len());

            if !reached_end && self.bit_reader.bytes_remaining() == 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }
        } else if let Some(head) = self.head.as_mut() {
            head.comment = core::ptr::null_mut();
        }

        self.mode = Mode::HCrc;
        self.hcrc()
    }

    fn hcrc(&mut self) -> ReturnCode {
        if (self.flags & 0x0200) != 0 {
            need_bits!(self, 16);

            if (self.wrap & 4) != 0 && self.bit_reader.hold() as u32 != (self.checksum & 0xffff) {
                self.mode = Mode::Bad;
                return self.bad("header crc mismatch\0");
            }

            self.bit_reader.init_bits();
        }

        if let Some(head) = self.head.as_mut() {
            head.hcrc = (self.flags >> 9) & 1;
            head.done = 1;
        }

        // compute crc32 checksum if not in raw mode
        if (self.wrap & 4 != 0) && self.flags != 0 {
            self.crc_fold = Crc32Fold::new();
            self.checksum = crate::CRC32_INITIAL_VALUE;
        }

        self.mode = Mode::Type;
        self.type_()
    }

    fn sync(&mut self) -> ReturnCode {
        ReturnCode::StreamError
    }

    fn lit(&mut self) -> ReturnCode {
        if self.writer.remaining() == 0 {
            #[cfg(all(test, feature = "std"))]
            eprintln!("Ok: read_buf is full ({} bytes)", self.writer.capacity());
            return self.inflate_leave(ReturnCode::Ok);
        }

        self.writer.push(self.length as u8);

        self.mode = Mode::Len;

        self.len()
    }

    fn check(&mut self) -> ReturnCode {
        if self.wrap != 0 {
            need_bits!(self, 32);

            self.total += self.writer.len();

            if self.wrap & 4 != 0 {
                if self.flags != 0 {
                    self.crc_fold.fold(self.writer.filled(), self.checksum);
                    self.checksum = self.crc_fold.finish();
                } else {
                    self.checksum = adler32(self.checksum, self.writer.filled());
                }
            }

            let given_checksum = if self.flags != 0 {
                self.bit_reader.hold() as u32
            } else {
                zswap32(self.bit_reader.hold() as u32)
            };

            self.out_available = self.writer.capacity() - self.writer.len();

            if self.wrap & 4 != 0 && given_checksum != self.checksum {
                self.mode = Mode::Bad;
                return self.bad("incorrect data check\0");
            }

            self.bit_reader.init_bits();
        }
        self.mode = Mode::Length;
        self.length()
    }

    fn length(&mut self) -> ReturnCode {
        // for gzip, last bytes contain LENGTH
        if self.wrap != 0 && self.flags != 0 {
            need_bits!(self, 32);
            if (self.wrap & 4) != 0 && self.bit_reader.hold() != self.total as u64 {
                self.mode = Mode::Bad;
                return self.bad("incorrect length check\0");
            }

            self.bit_reader.init_bits();
        }

        // inflate stream terminated properly
        self.inflate_leave(ReturnCode::StreamEnd)
    }

    fn type_(&mut self) -> ReturnCode {
        use InflateFlush::*;

        match self.flush {
            Block | Trees => self.inflate_leave(ReturnCode::Ok),
            NoFlush | SyncFlush | Finish => self.type_do(),
        }
    }

    fn type_do(&mut self) -> ReturnCode {
        if self.last {
            self.bit_reader.next_byte_boundary();
            self.mode = Mode::Check;
            return self.check();
        }

        need_bits!(self, 3);
        self.last = self.bit_reader.bits(1) != 0;
        self.bit_reader.drop_bits(1);

        match self.bit_reader.bits(2) {
            0 => {
                // eprintln!("inflate:     stored block (last = {last})");

                self.bit_reader.drop_bits(2);

                self.mode = Mode::Stored;
                self.stored()
            }
            1 => {
                // eprintln!("inflate:     fixed codes block (last = {last})");

                self.len_table = Table {
                    codes: Codes::Fixed(&self::inffixed_tbl::LENFIX),
                    bits: 9,
                };

                self.dist_table = Table {
                    codes: Codes::Fixed(&self::inffixed_tbl::DISTFIX),
                    bits: 5,
                };

                self.mode = Mode::Len_;

                self.bit_reader.drop_bits(2);

                if let InflateFlush::Trees = self.flush {
                    self.inflate_leave(ReturnCode::Ok)
                } else {
                    self.len_()
                }
            }
            2 => {
                // eprintln!("inflate:     dynamic codes block (last = {last})");

                self.bit_reader.drop_bits(2);

                self.mode = Mode::Table;
                self.table()
            }
            3 => {
                // eprintln!("inflate:     invalid block type");

                self.bit_reader.drop_bits(2);

                self.mode = Mode::Bad;
                self.bad("invalid block type\0")
            }
            _ => unsafe { core::hint::unreachable_unchecked() },
        }
    }

    fn stored(&mut self) -> ReturnCode {
        self.bit_reader.next_byte_boundary();

        need_bits!(self, 32);

        let hold = self.bit_reader.bits(32) as u32;

        // eprintln!("hold {hold:#x}");

        if hold as u16 != !((hold >> 16) as u16) {
            self.mode = Mode::Bad;
            return self.bad("invalid stored block lengths\0");
        }

        self.length = hold as usize & 0xFFFF;
        // eprintln!("inflate:     stored length {}", state.length);

        self.bit_reader.init_bits();

        if let InflateFlush::Trees = self.flush {
            self.inflate_leave(ReturnCode::Ok)
        } else {
            self.mode = Mode::CopyBlock;
            self.copy_block()
        }
    }

    fn copy_block(&mut self) -> ReturnCode {
        loop {
            let mut copy = self.length;

            if copy == 0 {
                break;
            }

            copy = Ord::min(copy, self.writer.remaining());
            copy = Ord::min(copy, self.bit_reader.bytes_remaining());

            if copy == 0 {
                return self.inflate_leave(ReturnCode::Ok);
            }

            self.writer.extend(&self.bit_reader.as_slice()[..copy]);
            self.bit_reader.advance(copy);

            self.length -= copy;
        }

        self.mode = Mode::Type;
        self.type_()
    }

    fn len_(&mut self) -> ReturnCode {
        self.mode = Mode::Len;
        self.len()
    }

    fn len(&mut self) -> ReturnCode {
        let avail_in = self.bit_reader.bytes_remaining();
        let avail_out = self.writer.remaining();

        // INFLATE_FAST_MIN_LEFT is important. It makes sure there is at least 32 bytes of free
        // space available. This means for many SIMD operations we don't need to process a
        // remainder; we just copy blindly, and a later operation will overwrite the extra copied
        // bytes
        if avail_in >= INFLATE_FAST_MIN_HAVE && avail_out >= INFLATE_FAST_MIN_LEFT {
            return inflate_fast_help(self, 0);
        }

        self.back = 0;

        // get a literal, length, or end-of-block code
        let mut here;
        loop {
            let bits = self.bit_reader.bits(self.len_table.bits);
            here = self.len_table_get(bits as usize);

            if here.bits <= self.bit_reader.bits_in_buffer() {
                break;
            }

            pull_byte!(self);
        }

        if here.op != 0 && here.op & 0xf0 == 0 {
            let last = here;
            loop {
                let bits = self.bit_reader.bits((last.bits + last.op) as usize) as u16;
                here = self.len_table_get((last.val + (bits >> last.bits)) as usize);
                if last.bits + here.bits <= self.bit_reader.bits_in_buffer() {
                    break;
                }

                pull_byte!(self);
            }

            self.bit_reader.drop_bits(last.bits as usize);
            self.back += last.bits as usize;
        }

        self.bit_reader.drop_bits(here.bits as usize);
        self.back += here.bits as usize;
        self.length = here.val as usize;

        if here.op == 0 {
            self.mode = Mode::Lit;
            self.lit()
        } else if here.op & 32 != 0 {
            // end of block

            // eprintln!("inflate:         end of block");

            self.back = usize::MAX;
            self.mode = Mode::Type;
            self.type_()
        } else if here.op & 64 != 0 {
            self.mode = Mode::Bad;
            self.bad("invalid literal/length code\0")
        } else {
            // length code
            self.extra = (here.op & MAX_BITS) as usize;
            self.mode = Mode::LenExt;
            self.len_ext()
        }
    }

    fn len_ext(&mut self) -> ReturnCode {
        let extra = self.extra;

        // get extra bits, if any
        if extra != 0 {
            need_bits!(self, extra);
            self.length += self.bit_reader.bits(extra) as usize;
            self.bit_reader.drop_bits(extra);
            self.back += extra;
        }

        // eprintln!("inflate: length {}", state.length);

        self.was = self.length;
        self.mode = Mode::Dist;
        self.dist()
    }

    fn dist(&mut self) -> ReturnCode {
        // get distance code
        let mut here;
        loop {
            let bits = self.bit_reader.bits(self.dist_table.bits) as usize;
            here = self.dist_table_get(bits);
            if here.bits <= self.bit_reader.bits_in_buffer() {
                break;
            }

            pull_byte!(self);
        }

        if here.op & 0xf0 == 0 {
            let last = here;

            loop {
                let bits = self.bit_reader.bits((last.bits + last.op) as usize);
                here = self.dist_table_get(last.val as usize + ((bits as usize) >> last.bits));

                if last.bits + here.bits <= self.bit_reader.bits_in_buffer() {
                    break;
                }

                pull_byte!(self);
            }

            self.bit_reader.drop_bits(last.bits as usize);
            self.back += last.bits as usize;
        }

        self.bit_reader.drop_bits(here.bits as usize);

        if here.op & 64 != 0 {
            self.mode = Mode::Bad;
            return self.bad("invalid distance code\0");
        }

        self.offset = here.val as usize;

        self.extra = (here.op & MAX_BITS) as usize;
        self.mode = Mode::DistExt;
        self.dist_ext()
    }

    fn dist_ext(&mut self) -> ReturnCode {
        let extra = self.extra;

        if extra > 0 {
            need_bits!(self, extra);
            self.offset += self.bit_reader.bits(extra) as usize;
            self.bit_reader.drop_bits(extra);
            self.back += extra;
        }

        if self.offset > self.dmax {
            self.mode = Mode::Bad;
            return self.bad("invalid distance code too far back\0");
        }

        // eprintln!("inflate: distance {}", state.offset);

        self.mode = Mode::Match;
        self.match_()
    }

    /// copy match from window to output

    fn match_(&mut self) -> ReturnCode {
        if self.writer.remaining() == 0 {
            #[cfg(all(feature = "std", test))]
            eprintln!(
                "BufError: read_buf is full ({} bytes)",
                self.writer.capacity()
            );
            return self.inflate_leave(ReturnCode::Ok);
        }

        // this is not quite right. not sure when that matters
        let out = self.writer.remaining() + self.writer.len();
        let left = self.writer.remaining();

        let copy = out - left;

        let copy = if self.offset > copy {
            // copy from window to output

            let mut copy = self.offset - copy;

            if copy > self.window.have() {
                if self.sane {
                    self.mode = Mode::Bad;
                    return self.bad("invalid distance too far back\0");
                }

                // TODO INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR
                panic!("INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR")
            }

            let wnext = self.window.next();
            let wsize = self.window.size();

            let from = if copy > wnext {
                copy -= wnext;
                wsize - copy
            } else {
                wnext - copy
            };

            copy = Ord::min(copy, self.length);
            copy = Ord::min(copy, left);

            self.writer.extend(&self.window.as_slice()[from..][..copy]);

            copy
        } else {
            let copy = Ord::min(self.length, self.writer.remaining());
            self.writer.copy_match(self.offset, copy);

            copy
        };

        self.length -= copy;

        if self.length == 0 {
            self.mode = Mode::Len;
            self.len()
        } else {
            // otherwise it seems to recurse?
            self.match_()
        }
    }

    /// get dynamic table entries descriptor

    fn table(&mut self) -> ReturnCode {
        need_bits!(self, 14);
        self.nlen = self.bit_reader.bits(5) as usize + 257;
        self.bit_reader.drop_bits(5);
        self.ndist = self.bit_reader.bits(5) as usize + 1;
        self.bit_reader.drop_bits(5);
        self.ncode = self.bit_reader.bits(4) as usize + 4;
        self.bit_reader.drop_bits(4);

        // TODO pkzit_bug_workaround
        if self.nlen > 286 || self.ndist > 30 {
            self.mode = Mode::Bad;
            return self.bad("too many length or distance symbols\0");
        }

        self.have = 0;
        self.mode = Mode::LenLens;
        self.len_lens()
    }

    /// get code length code lengths (not a typo)

    fn len_lens(&mut self) -> ReturnCode {
        // permutation of code lengths ;
        const ORDER: [u16; 19] = [
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
        ];

        while self.have < self.ncode {
            need_bits!(self, 3);
            self.lens[ORDER[self.have] as usize] = self.bit_reader.bits(3) as u16;
            self.have += 1;
            self.bit_reader.drop_bits(3);
        }

        while self.have < 19 {
            self.lens[ORDER[self.have] as usize] = 0;
            self.have += 1;
        }

        self.len_table.bits = 7;

        let InflateTable::Success(root) = inflate_table(
            CodeType::Codes,
            &self.lens,
            19,
            &mut self.codes_codes,
            self.len_table.bits,
            &mut self.work,
        ) else {
            self.mode = Mode::Bad;
            return self.bad("invalid code lengths set\0");
        };

        self.len_table.codes = Codes::Codes;
        self.len_table.bits = root;

        self.have = 0;
        self.mode = Mode::CodeLens;
        self.code_lens()
    }

    /// get length and distance code code lengths

    fn code_lens(&mut self) -> ReturnCode {
        while self.have < self.nlen + self.ndist {
            let here = loop {
                let bits = self.bit_reader.bits(self.len_table.bits);
                let here = self.len_table_get(bits as usize);
                if here.bits <= self.bit_reader.bits_in_buffer() {
                    break here;
                }

                pull_byte!(self);
            };

            let here_bits = here.bits as usize;

            match here.val {
                0..=15 => {
                    self.bit_reader.drop_bits(here_bits);
                    self.lens[self.have] = here.val;
                    self.have += 1;
                }
                16 => {
                    need_bits!(self, here_bits + 2);
                    self.bit_reader.drop_bits(here_bits);
                    if self.have == 0 {
                        self.mode = Mode::Bad;
                        return self.bad("invalid bit length repeat\0");
                    }

                    let len = self.lens[self.have - 1];
                    let copy = 3 + self.bit_reader.bits(2) as usize;
                    self.bit_reader.drop_bits(2);

                    if self.have + copy > self.nlen + self.ndist {
                        self.mode = Mode::Bad;
                        return self.bad("invalid bit length repeat\0");
                    }

                    for _ in 0..copy {
                        self.lens[self.have] = len;
                        self.have += 1;
                    }
                }
                17 => {
                    need_bits!(self, here_bits + 3);
                    self.bit_reader.drop_bits(here_bits);
                    let len = 0;
                    let copy = 3 + self.bit_reader.bits(3) as usize;
                    self.bit_reader.drop_bits(3);

                    if self.have + copy > self.nlen + self.ndist {
                        self.mode = Mode::Bad;
                        return self.bad("invalid bit length repeat\0");
                    }

                    for _ in 0..copy {
                        self.lens[self.have] = len as u16;
                        self.have += 1;
                    }
                }
                18.. => {
                    need_bits!(self, here_bits + 7);
                    self.bit_reader.drop_bits(here_bits);
                    let len = 0;
                    let copy = 11 + self.bit_reader.bits(7) as usize;
                    self.bit_reader.drop_bits(7);

                    if self.have + copy > self.nlen + self.ndist {
                        self.mode = Mode::Bad;
                        return self.bad("invalid bit length repeat\0");
                    }

                    for _ in 0..copy {
                        self.lens[self.have] = len as u16;
                        self.have += 1;
                    }
                }
            }
        }

        // check for end-of-block code (better have one)
        if self.lens[256] == 0 {
            self.mode = Mode::Bad;
            return self.bad("invalid code -- missing end-of-block\0");
        }

        // build code tables

        self.len_table.bits = 10;

        let InflateTable::Success(root) = inflate_table(
            CodeType::Lens,
            &self.lens,
            self.nlen,
            &mut self.len_codes,
            self.len_table.bits,
            &mut self.work,
        ) else {
            self.mode = Mode::Bad;
            return self.bad("invalid literal/lengths set\0");
        };

        self.len_table.codes = Codes::Len;
        self.len_table.bits = root;

        self.dist_table.bits = 9;

        let InflateTable::Success(root) = inflate_table(
            CodeType::Dists,
            &self.lens[self.nlen..],
            self.ndist,
            &mut self.dist_codes,
            self.dist_table.bits,
            &mut self.work,
        ) else {
            self.mode = Mode::Bad;
            return self.bad("invalid distances set\0");
        };

        self.dist_table.bits = root;
        self.dist_table.codes = Codes::Dist;

        self.mode = Mode::Len_;

        if matches!(self.flush, InflateFlush::Trees) {
            return self.inflate_leave(ReturnCode::Ok);
        }

        self.len_()
    }

    fn dict_id(&mut self) -> ReturnCode {
        need_bits!(self, 32);

        self.checksum = zswap32(self.bit_reader.hold() as u32);

        self.bit_reader.init_bits();

        self.mode = Mode::Dict;
        self.dict()
    }

    fn dict(&mut self) -> ReturnCode {
        if !self.havedict {
            return self.inflate_leave(ReturnCode::NeedDict);
        }

        self.checksum = crate::ADLER32_INITIAL_VALUE as _;

        self.mode = Mode::Type;
        self.type_()
    }

    fn mem(&mut self) -> ReturnCode {
        self.inflate_leave(ReturnCode::MemError)
    }

    fn bad(&mut self, msg: &'static str) -> ReturnCode {
        #[cfg(all(feature = "std", test))]
        dbg!(msg);
        self.error_message = Some(msg);
        self.inflate_leave(ReturnCode::DataError)
    }

    // NOTE: it is crucial for the internal bookkeeping that this is the only route for actually
    // leaving the inflate function call chain
    fn inflate_leave(&mut self, return_code: ReturnCode) -> ReturnCode {
        // actual logic is in `inflate` itself
        return_code
    }

    /// Stored in the `z_stream.data_type` field
    fn decoding_state(&self) -> i32 {
        let bit_reader_bits = self.bit_reader.bits_in_buffer() as i32;
        debug_assert!(bit_reader_bits < 64);

        let last = if self.last { 64 } else { 0 };

        let mode = match self.mode {
            Mode::Type => 128,
            Mode::Len_ | Mode::CopyBlock => 256,
            _ => 0,
        };

        bit_reader_bits | last | mode
    }
}

fn inflate_fast_help(state: &mut State, _start: usize) -> ReturnCode {
    let mut bit_reader = BitReader::new(&[]);
    core::mem::swap(&mut bit_reader, &mut state.bit_reader);

    let mut writer = ReadBuf::new(&mut []);
    core::mem::swap(&mut writer, &mut state.writer);

    let lcode = state.len_table_ref();
    let dcode = state.dist_table_ref();

    // IDEA: use const generics for the bits here?
    let lmask = (1u64 << state.len_table.bits) - 1;
    let dmask = (1u64 << state.dist_table.bits) - 1;

    // TODO verify if this is relevant for us
    let extra_safe = false;

    let window_size = state.window.size();

    let mut bad = None;

    if bit_reader.bits_in_buffer() < 10 {
        bit_reader.refill();
    }

    'outer: loop {
        let mut here = bit_reader.refill_and(|hold| lcode[(hold & lmask) as usize]);

        if here.op == 0 {
            writer.push(here.val as u8);
            bit_reader.drop_bits(here.bits as usize);
            here = lcode[(bit_reader.hold() & lmask) as usize];

            if here.op == 0 {
                writer.push(here.val as u8);
                bit_reader.drop_bits(here.bits as usize);
                here = lcode[(bit_reader.hold() & lmask) as usize];
            }
        }

        'dolen: loop {
            bit_reader.drop_bits(here.bits as usize);
            let op = here.op;

            if op == 0 {
                writer.push(here.val as u8);
            } else if op & 16 != 0 {
                let op = op & MAX_BITS;
                let mut len = here.val + bit_reader.bits(op as usize) as u16;
                bit_reader.drop_bits(op as usize);

                here = dcode[(bit_reader.hold() & dmask) as usize];

                // we have two fast-path loads: 10+10 + 15+5 = 40,
                // but we may need to refill here in the worst case
                if bit_reader.bits_in_buffer() < MAX_BITS + MAX_DIST_EXTRA_BITS {
                    bit_reader.refill();
                }

                'dodist: loop {
                    bit_reader.drop_bits(here.bits as usize);
                    let op = here.op;

                    if op & 16 != 0 {
                        let op = op & MAX_BITS;
                        let dist = here.val + bit_reader.bits(op as usize) as u16;

                        if dist as usize > state.dmax {
                            bad = Some("invalid distance too far back\0");
                            state.mode = Mode::Bad;
                            break 'outer;
                        }

                        bit_reader.drop_bits(op as usize);

                        // max distance in output
                        let written = writer.len();

                        if dist as usize > written {
                            // copy fropm the window
                            if (dist as usize - written) > state.window.have() {
                                if state.sane {
                                    bad = Some("invalid distance too far back\0");
                                    state.mode = Mode::Bad;
                                    break 'outer;
                                }

                                panic!("INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR")
                            }

                            let mut op = dist as usize - written;
                            let mut from;

                            let window_next = state.window.next();

                            if window_next == 0 {
                                // This case is hit when the window has just wrapped around
                                // by logic in `Window::extend`. It is special-cased because
                                // apparently this is quite common.
                                //
                                // the match is at the end of the window, even though the next
                                // position has now wrapped around.
                                from = window_size - op;
                            } else if window_next >= op {
                                // the standard case: a contiguous copy from the window, no wrapping
                                from = window_next - op;
                            } else {
                                // This case is hit when the window has recently wrapped around
                                // by logic in `Window::extend`.
                                //
                                // The match is (partially) at the end of the window
                                op -= window_next;
                                from = window_size - op;

                                if op < len as usize {
                                    // This case is hit when part of the match is at the end of the
                                    // window, and part of it has wrapped around to the start. Copy
                                    // the end section here, the start section will be copied below.
                                    len -= op as u16;
                                    writer.extend(&state.window.as_slice()[from..][..op]);
                                    from = 0;
                                    op = window_next;
                                }
                            }

                            let copy = Ord::min(op, len as usize);
                            writer.extend(&state.window.as_slice()[from..][..copy]);

                            if op < len as usize {
                                // here we need some bytes from the output itself
                                writer.copy_match(dist as usize, len as usize - op);
                            }
                        } else if extra_safe {
                            todo!()
                        } else {
                            writer.copy_match(dist as usize, len as usize)
                        }
                    } else if (op & 64) == 0 {
                        // 2nd level distance code
                        here = dcode[(here.val + bit_reader.bits(op as usize) as u16) as usize];
                        continue 'dodist;
                    } else {
                        bad = Some("invalid distance code\0");
                        state.mode = Mode::Bad;
                        break 'outer;
                    }

                    break 'dodist;
                }
            } else if (op & 64) == 0 {
                // 2nd level length code
                here = lcode[(here.val + bit_reader.bits(op as usize) as u16) as usize];
                continue 'dolen;
            } else if op & 32 != 0 {
                // end of block
                state.mode = Mode::Type;
                break 'outer;
            } else {
                bad = Some("invalid literal/length code\0");
                state.mode = Mode::Bad;
                break 'outer;
            }

            break 'dolen;
        }

        let remaining = bit_reader.bytes_remaining();
        if remaining.saturating_sub(INFLATE_FAST_MIN_LEFT - 1) > 0
            && writer.remaining() > INFLATE_FAST_MIN_LEFT
        {
            continue;
        }

        break 'outer;
    }

    // return unused bytes (on entry, bits < 8, so in won't go too far back)
    bit_reader.return_unused_bytes();

    state.bit_reader = bit_reader;
    state.writer = writer;

    match state.mode {
        Mode::Type => state.type_(),
        Mode::Len => state.len(),
        Mode::Bad => state.bad(bad.unwrap()),
        _ => unreachable!(),
    }
}

pub fn prime(stream: &mut InflateStream, bits: i32, value: i32) -> ReturnCode {
    if bits == 0 {
        /* fall through */
    } else if bits < 0 {
        stream.state.bit_reader.init_bits();
    } else if bits > 16 || stream.state.bit_reader.bits_in_buffer() + bits as u8 > 32 {
        return ReturnCode::StreamError;
    } else {
        stream.state.bit_reader.prime(bits as u8, value as u64);
    }

    ReturnCode::Ok
}

#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub struct InflateConfig {
    pub window_bits: i32,
}

impl Default for InflateConfig {
    fn default() -> Self {
        Self {
            window_bits: DEF_WBITS,
        }
    }
}

/// Initialize the stream in an inflate state
pub fn init(stream: &mut z_stream, config: InflateConfig) -> ReturnCode {
    stream.msg = core::ptr::null_mut();

    // for safety we must really make sure that alloc and free are consistent
    // this is a (slight) deviation from stock zlib. In this crate we pick the rust
    // allocator as the default, but `libz-rs-sys` configures the C allocator
    #[cfg(feature = "rust-allocator")]
    if stream.zalloc.is_none() || stream.zfree.is_none() {
        stream.configure_default_rust_allocator()
    }

    #[cfg(feature = "c-allocator")]
    if stream.zalloc.is_none() || stream.zfree.is_none() {
        stream.configure_default_c_allocator()
    }

    if stream.zalloc.is_none() || stream.zfree.is_none() {
        return ReturnCode::StreamError;
    }

    let mut state = State::new(&[], ReadBuf::new(&mut []));

    // TODO this can change depending on the used/supported SIMD instructions
    state.chunksize = 32;

    let alloc = Allocator {
        zalloc: stream.zalloc.unwrap(),
        zfree: stream.zfree.unwrap(),
        opaque: stream.opaque,
        _marker: PhantomData,
    };

    // allocated here to have the same order as zlib
    let Some(state_allocation) = alloc.allocate::<State>() else {
        return ReturnCode::MemError;
    };

    stream.state = state_allocation.write(state) as *mut _ as *mut internal_state;

    // SAFETY: we've correctly initialized the stream to be an InflateStream
    let ret = if let Some(stream) = unsafe { InflateStream::from_stream_mut(stream) } {
        reset_with_config(stream, config)
    } else {
        ReturnCode::StreamError
    };

    if ret != ReturnCode::Ok {
        let ptr = stream.state;
        stream.state = core::ptr::null_mut();
        // SAFETY: we assume deallocation does not cause UB
        unsafe { alloc.deallocate(ptr, 1) };
    }

    ret
}

pub fn reset_with_config(stream: &mut InflateStream, config: InflateConfig) -> ReturnCode {
    let mut window_bits = config.window_bits;
    let wrap;

    if window_bits < 0 {
        wrap = 0;

        if window_bits < -MAX_WBITS {
            return ReturnCode::StreamError;
        }

        window_bits = -window_bits;
    } else {
        wrap = (window_bits >> 4) + 5; // TODO wth?

        if window_bits < 48 {
            window_bits &= MAX_WBITS;
        }
    }

    if window_bits != 0 && !(MIN_WBITS..=MAX_WBITS).contains(&window_bits) {
        #[cfg(feature = "std")]
        eprintln!("invalid windowBits");
        return ReturnCode::StreamError;
    }

    if stream.state.window.size() != 0 && stream.state.wbits != window_bits as usize {
        let mut window = Window::empty();
        core::mem::swap(&mut window, &mut stream.state.window);

        let window = window.into_inner();
        assert!(!window.is_empty());
        unsafe { stream.alloc.deallocate(window.as_mut_ptr(), window.len()) };
    }

    stream.state.wrap = wrap as usize;
    stream.state.wbits = window_bits as _;

    reset(stream)
}

pub fn reset(stream: &mut InflateStream) -> ReturnCode {
    // reset the state of the window
    stream.state.window.clear();

    stream.state.error_message = None;

    reset_keep(stream)
}

pub fn reset_keep(stream: &mut InflateStream) -> ReturnCode {
    stream.total_in = 0;
    stream.total_out = 0;
    stream.state.total = 0;

    stream.msg = core::ptr::null_mut();

    let state = &mut stream.state;

    if state.wrap != 0 {
        // to support ill-conceived Java test suite
        stream.adler = (state.wrap & 1) as _;
    }

    state.mode = Mode::Head;
    state.checksum = crate::ADLER32_INITIAL_VALUE as u32;

    state.last = false;
    state.havedict = false;
    state.flags = -1;
    state.dmax = 32768;
    state.head = None;
    state.bit_reader = BitReader::new(&[]);

    state.next = 0;
    state.len_table = Table::default();
    state.dist_table = Table::default();

    state.sane = true;
    state.back = usize::MAX;

    ReturnCode::Ok
}

pub unsafe fn inflate(stream: &mut InflateStream, flush: InflateFlush) -> ReturnCode {
    if stream.next_out.is_null() || (stream.next_in.is_null() && stream.avail_in != 0) {
        return ReturnCode::StreamError as _;
    }

    let source_slice = core::slice::from_raw_parts(stream.next_in, stream.avail_in as usize);
    let dest_slice = core::slice::from_raw_parts_mut(stream.next_out, stream.avail_out as usize);

    let state = &mut stream.state;

    // skip check
    if let Mode::Type = state.mode {
        state.mode = Mode::TypeDo;
    }

    state.flush = flush;

    state.bit_reader.update_slice(source_slice);
    state.writer = ReadBuf::new(dest_slice);

    state.in_available = stream.avail_in as _;
    state.out_available = stream.avail_out as _;

    let mut err = state.dispatch();

    let in_read = state.bit_reader.as_ptr() as usize - stream.next_in as usize;
    let out_written = state.out_available - (state.writer.capacity() - state.writer.len());

    stream.total_in += in_read as z_size;
    state.total += out_written;
    stream.total_out = state.total as _;

    stream.avail_in = state.bit_reader.bytes_remaining() as u32;
    stream.next_in = state.bit_reader.as_ptr() as *mut u8;

    stream.avail_out = (state.writer.capacity() - state.writer.len()) as u32;
    stream.next_out = state.writer.next_out() as *mut u8;

    stream.adler = state.checksum as z_checksum;

    let valid_mode = |mode| !matches!(mode, Mode::Bad | Mode::Mem | Mode::Sync);
    let not_done = |mode| {
        !matches!(
            mode,
            Mode::Check | Mode::Length | Mode::Bad | Mode::Mem | Mode::Sync
        )
    };

    let must_update_window = state.window.size() != 0
        || (out_written != 0
            && valid_mode(state.mode)
            && (not_done(state.mode) || !matches!(state.flush, InflateFlush::Finish)));

    let update_checksum = state.wrap & 4 != 0;

    if must_update_window {
        'blk: {
            // initialize the window if needed
            if state.window.size() == 0 {
                match Window::new_in(&stream.alloc, state.wbits) {
                    Some(window) => state.window = window,
                    None => {
                        state.mode = Mode::Mem;
                        err = ReturnCode::MemError;
                        break 'blk;
                    }
                }
            }

            state.window.extend(
                &state.writer.filled()[..out_written],
                state.flags,
                update_checksum,
                &mut state.checksum,
                &mut state.crc_fold,
            );
        }
    }

    if let Some(msg) = state.error_message {
        assert!(msg.ends_with(|c| c == '\0'));
        stream.msg = msg.as_ptr() as *mut u8 as *mut core::ffi::c_char;
    }

    stream.data_type = state.decoding_state();

    if ((in_read == 0 && out_written == 0) || flush == InflateFlush::Finish as _)
        && err == (ReturnCode::Ok as _)
    {
        ReturnCode::BufError as _
    } else {
        err as _
    }
}

fn syncsearch(mut got: usize, buf: &[u8]) -> (usize, usize) {
    let len = buf.len();
    let mut next = 0;

    while next < len && got < 4 {
        if buf[next] == if got < 2 { 0 } else { 0xff } {
            got += 1;
        } else if buf[next] != 0 {
            got = 0;
        } else {
            got = 4 - got;
        }
        next += 1;
    }

    (got, next)
}

pub fn sync(stream: &mut InflateStream) -> ReturnCode {
    let state = &mut stream.state;

    if stream.avail_in == 0 && state.bit_reader.bits_in_buffer() < 8 {
        return ReturnCode::BufError;
    }
    /* if first time, start search in bit buffer */
    if !matches!(state.mode, Mode::Sync) {
        state.mode = Mode::Sync;

        let (buf, len) = state.bit_reader.start_sync_search();

        (state.have, _) = syncsearch(0, &buf[..len]);
    }

    // search available input
    let slice = unsafe { core::slice::from_raw_parts(stream.next_in, stream.avail_in as usize) };

    let len;
    (state.have, len) = syncsearch(state.have, slice);
    stream.next_in = unsafe { stream.next_in.add(len) };
    stream.avail_in -= len as u32;
    stream.total_in += len as z_size;

    /* return no joy or set up to restart inflate() on a new block */
    if state.have != 4 {
        return ReturnCode::DataError;
    }

    if state.flags == -1 {
        state.wrap = 0; /* if no header yet, treat as raw */
    } else {
        state.wrap &= !4; /* no point in computing a check value now */
    }

    let flags = state.flags;
    let total_in = stream.total_in;
    let total_out = stream.total_out;

    reset(stream);

    stream.total_in = total_in;
    stream.total_out = total_out;

    stream.state.flags = flags;
    stream.state.mode = Mode::Type;

    ReturnCode::Ok
}

/*
  Returns true if inflate is currently at the end of a block generated by
  Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
  implementation to provide an additional safety check. PPP uses
  Z_SYNC_FLUSH but removes the length bytes of the resulting empty stored
  block. When decompressing, PPP checks that at the end of input packet,
  inflate is waiting for these length bytes.
*/
pub fn sync_point(stream: &mut InflateStream) -> bool {
    matches!(stream.state.mode, Mode::Stored) && stream.state.bit_reader.bits_in_buffer() == 0
}

pub unsafe fn copy<'a>(
    dest: &mut MaybeUninit<InflateStream<'a>>,
    source: &InflateStream<'a>,
) -> ReturnCode {
    if source.next_out.is_null() || (source.next_in.is_null() && source.avail_in != 0) {
        return ReturnCode::StreamError;
    }

    // Safety: source and dest are both mutable references, so guaranteed not to overlap.
    // dest being a reference to maybe uninitialized memory makes a copy of 1 DeflateStream valid.
    unsafe {
        core::ptr::copy_nonoverlapping(source, dest.as_mut_ptr(), 1);
    }

    // allocated here to have the same order as zlib
    let Some(state_allocation) = source.alloc.allocate::<State>() else {
        return ReturnCode::MemError;
    };

    let state = &source.state;

    let writer: MaybeUninit<ReadBuf> =
        unsafe { core::ptr::read(&state.writer as *const _ as *const MaybeUninit<ReadBuf>) };

    let mut copy = State {
        mode: state.mode,
        last: state.last,
        wrap: state.wrap,
        len_table: state.len_table,
        dist_table: state.dist_table,
        wbits: state.wbits,
        window: Window::empty(),
        head: None,
        ncode: state.ncode,
        nlen: state.nlen,
        ndist: state.ndist,
        have: state.have,
        next: state.next,
        bit_reader: state.bit_reader,
        writer: ReadBuf::new(&mut []),
        total: state.total,
        length: state.length,
        offset: state.offset,
        extra: state.extra,
        sane: state.sane,
        back: state.back,
        was: state.was,
        chunksize: state.chunksize,
        in_available: state.in_available,
        out_available: state.out_available,
        lens: state.lens,
        work: state.work,
        error_message: state.error_message,
        flush: state.flush,
        checksum: state.checksum,
        crc_fold: state.crc_fold,
        havedict: state.havedict,
        dmax: state.dmax,
        flags: state.flags,
        codes_codes: state.codes_codes,
        len_codes: state.len_codes,
        dist_codes: state.dist_codes,
    };

    if !state.window.is_empty() {
        let Some(window) = state.window.clone_in(&source.alloc) else {
            source.alloc.deallocate(state_allocation.as_mut_ptr(), 1);
            return ReturnCode::MemError;
        };

        copy.window = window;
    }

    // write the cloned state into state_ptr
    let state_ptr = state_allocation.write(copy);

    // insert the state_ptr into `dest`
    let field_ptr = unsafe { core::ptr::addr_of_mut!((*dest.as_mut_ptr()).state) };
    unsafe { core::ptr::write(field_ptr as *mut *mut State, state_ptr) };

    // update the writer; it cannot be cloned so we need to use some shennanigans
    let field_ptr = unsafe { core::ptr::addr_of_mut!((*dest.as_mut_ptr()).state.writer) };
    unsafe { core::ptr::copy(writer.as_ptr(), field_ptr, 1) };

    // update the gzhead field (it contains a mutable reference so we need to be careful
    let field_ptr = unsafe { core::ptr::addr_of_mut!((*dest.as_mut_ptr()).state.head) };
    unsafe { core::ptr::copy(&source.state.head, field_ptr, 1) };

    ReturnCode::Ok
}

pub fn undermine(stream: &mut InflateStream, subvert: i32) -> ReturnCode {
    stream.state.sane = (!subvert) != 0;

    ReturnCode::Ok
}

pub fn mark(stream: &InflateStream) -> c_long {
    if stream.next_out.is_null() || (stream.next_in.is_null() && stream.avail_in != 0) {
        return c_long::MIN;
    }

    let state = &stream.state;

    let length = match state.mode {
        Mode::CopyBlock => state.length,
        Mode::Match => state.was - state.length,
        _ => 0,
    };

    (((state.back as c_long) as c_ulong) << 16) as c_long + length as c_long
}

pub fn set_dictionary(stream: &mut InflateStream, dictionary: &[u8]) -> ReturnCode {
    if stream.state.wrap != 0 && !matches!(stream.state.mode, Mode::Dict) {
        return ReturnCode::StreamError;
    }

    // check for correct dictionary identifier
    if matches!(stream.state.mode, Mode::Dict) {
        let dictid = adler32(1, dictionary);

        if dictid != stream.state.checksum {
            return ReturnCode::DataError;
        }
    }

    let err = 'blk: {
        // initialize the window if needed
        if stream.state.window.size() == 0 {
            match Window::new_in(&stream.alloc, stream.state.wbits) {
                None => break 'blk ReturnCode::MemError,
                Some(window) => stream.state.window = window,
            }
        }

        stream.state.window.extend(
            dictionary,
            stream.state.flags,
            false,
            &mut stream.state.checksum,
            &mut stream.state.crc_fold,
        );

        ReturnCode::Ok
    };

    if err != ReturnCode::Ok {
        stream.state.mode = Mode::Mem;
        return ReturnCode::MemError;
    }

    stream.state.havedict = true;

    ReturnCode::Ok
}

pub fn end<'a>(stream: &'a mut InflateStream<'a>) -> &'a mut z_stream {
    let alloc = stream.alloc;

    let mut window = Window::empty();
    core::mem::swap(&mut window, &mut stream.state.window);

    // safety: window is not used again
    if !window.is_empty() {
        let window = window.into_inner();
        unsafe { alloc.deallocate(window.as_mut_ptr(), window.len()) };
    }

    let stream = stream.as_z_stream_mut();

    let state_ptr = core::mem::replace(&mut stream.state, core::ptr::null_mut());

    // safety: state_ptr is not used again
    unsafe { alloc.deallocate(state_ptr as *mut State, 1) };

    stream
}

pub fn get_header<'a>(
    stream: &mut InflateStream<'a>,
    head: Option<&'a mut gz_header>,
) -> ReturnCode {
    if (stream.state.wrap & 2) == 0 {
        return ReturnCode::StreamError;
    }

    stream.state.head = head.map(|head| {
        head.done = 0;
        head
    });
    ReturnCode::Ok
}
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn uncompress_buffer_overflow() {
        let mut output = [0; 1 << 13];
        let input = [
            72, 137, 58, 0, 3, 39, 255, 255, 255, 255, 255, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
            14, 14, 184, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 184, 14, 14,
            14, 14, 14, 14, 14, 63, 14, 14, 14, 14, 14, 14, 14, 14, 184, 14, 14, 255, 14, 103, 14,
            14, 14, 14, 14, 14, 61, 14, 255, 255, 63, 14, 14, 14, 14, 14, 14, 14, 14, 184, 14, 14,
            255, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 6, 14, 14, 14, 14, 14, 14, 14, 14, 71,
            4, 137, 106,
        ];

        let config = InflateConfig { window_bits: 15 };

        let (_decompressed, err) = uncompress_slice(&mut output, &input, config);
        assert_eq!(err, ReturnCode::DataError);
    }
}
