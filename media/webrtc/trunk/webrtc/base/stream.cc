/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_POSIX)
#include <sys/file.h>
#endif  // WEBRTC_POSIX
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>
#include "webrtc/base/basictypes.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"
#define fileno _fileno
#endif

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// StreamInterface
///////////////////////////////////////////////////////////////////////////////
StreamInterface::~StreamInterface() {
}

StreamResult StreamInterface::WriteAll(const void* data, size_t data_len,
                                       size_t* written, int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_written = 0, current_written;
  while (total_written < data_len) {
    result = Write(static_cast<const char*>(data) + total_written,
                   data_len - total_written, &current_written, error);
    if (result != SR_SUCCESS)
      break;
    total_written += current_written;
  }
  if (written)
    *written = total_written;
  return result;
}

StreamResult StreamInterface::ReadAll(void* buffer, size_t buffer_len,
                                      size_t* read, int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_read = 0, current_read;
  while (total_read < buffer_len) {
    result = Read(static_cast<char*>(buffer) + total_read,
                  buffer_len - total_read, &current_read, error);
    if (result != SR_SUCCESS)
      break;
    total_read += current_read;
  }
  if (read)
    *read = total_read;
  return result;
}

StreamResult StreamInterface::ReadLine(std::string* line) {
  line->clear();
  StreamResult result = SR_SUCCESS;
  while (true) {
    char ch;
    result = Read(&ch, sizeof(ch), NULL, NULL);
    if (result != SR_SUCCESS) {
      break;
    }
    if (ch == '\n') {
      break;
    }
    line->push_back(ch);
  }
  if (!line->empty()) {   // give back the line we've collected so far with
    result = SR_SUCCESS;  // a success code.  Otherwise return the last code
  }
  return result;
}

void StreamInterface::PostEvent(Thread* t, int events, int err) {
  t->Post(this, MSG_POST_EVENT, new StreamEventData(events, err));
}

void StreamInterface::PostEvent(int events, int err) {
  PostEvent(Thread::Current(), events, err);
}

StreamInterface::StreamInterface() {
}

void StreamInterface::OnMessage(Message* msg) {
  if (MSG_POST_EVENT == msg->message_id) {
    StreamEventData* pe = static_cast<StreamEventData*>(msg->pdata);
    SignalEvent(this, pe->events, pe->error);
    delete msg->pdata;
  }
}

///////////////////////////////////////////////////////////////////////////////
// StreamAdapterInterface
///////////////////////////////////////////////////////////////////////////////

StreamAdapterInterface::StreamAdapterInterface(StreamInterface* stream,
                                               bool owned)
    : stream_(stream), owned_(owned) {
  if (NULL != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

void StreamAdapterInterface::Attach(StreamInterface* stream, bool owned) {
  if (NULL != stream_)
    stream_->SignalEvent.disconnect(this);
  if (owned_)
    delete stream_;
  stream_ = stream;
  owned_ = owned;
  if (NULL != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

StreamInterface* StreamAdapterInterface::Detach() {
  if (NULL != stream_)
    stream_->SignalEvent.disconnect(this);
  StreamInterface* stream = stream_;
  stream_ = NULL;
  return stream;
}

StreamAdapterInterface::~StreamAdapterInterface() {
  if (owned_)
    delete stream_;
}

///////////////////////////////////////////////////////////////////////////////
// StreamTap
///////////////////////////////////////////////////////////////////////////////

StreamTap::StreamTap(StreamInterface* stream, StreamInterface* tap)
    : StreamAdapterInterface(stream), tap_(), tap_result_(SR_SUCCESS),
        tap_error_(0) {
  AttachTap(tap);
}

void StreamTap::AttachTap(StreamInterface* tap) {
  tap_.reset(tap);
}

StreamInterface* StreamTap::DetachTap() {
  return tap_.release();
}

StreamResult StreamTap::GetTapResult(int* error) {
  if (error) {
    *error = tap_error_;
  }
  return tap_result_;
}

StreamResult StreamTap::Read(void* buffer, size_t buffer_len,
                             size_t* read, int* error) {
  size_t backup_read;
  if (!read) {
    read = &backup_read;
  }
  StreamResult res = StreamAdapterInterface::Read(buffer, buffer_len,
                                                  read, error);
  if ((res == SR_SUCCESS) && (tap_result_ == SR_SUCCESS)) {
    tap_result_ = tap_->WriteAll(buffer, *read, NULL, &tap_error_);
  }
  return res;
}

StreamResult StreamTap::Write(const void* data, size_t data_len,
                              size_t* written, int* error) {
  size_t backup_written;
  if (!written) {
    written = &backup_written;
  }
  StreamResult res = StreamAdapterInterface::Write(data, data_len,
                                                   written, error);
  if ((res == SR_SUCCESS) && (tap_result_ == SR_SUCCESS)) {
    tap_result_ = tap_->WriteAll(data, *written, NULL, &tap_error_);
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////////
// StreamSegment
///////////////////////////////////////////////////////////////////////////////

StreamSegment::StreamSegment(StreamInterface* stream)
    : StreamAdapterInterface(stream), start_(SIZE_UNKNOWN), pos_(0),
    length_(SIZE_UNKNOWN) {
  // It's ok for this to fail, in which case start_ is left as SIZE_UNKNOWN.
  stream->GetPosition(&start_);
}

StreamSegment::StreamSegment(StreamInterface* stream, size_t length)
    : StreamAdapterInterface(stream), start_(SIZE_UNKNOWN), pos_(0),
    length_(length) {
  // It's ok for this to fail, in which case start_ is left as SIZE_UNKNOWN.
  stream->GetPosition(&start_);
}

StreamResult StreamSegment::Read(void* buffer, size_t buffer_len,
                                 size_t* read, int* error) {
  if (SIZE_UNKNOWN != length_) {
    if (pos_ >= length_)
      return SR_EOS;
    buffer_len = _min(buffer_len, length_ - pos_);
  }
  size_t backup_read;
  if (!read) {
    read = &backup_read;
  }
  StreamResult result = StreamAdapterInterface::Read(buffer, buffer_len,
                                                     read, error);
  if (SR_SUCCESS == result) {
    pos_ += *read;
  }
  return result;
}

bool StreamSegment::SetPosition(size_t position) {
  if (SIZE_UNKNOWN == start_)
    return false;  // Not seekable
  if ((SIZE_UNKNOWN != length_) && (position > length_))
    return false;  // Seek past end of segment
  if (!StreamAdapterInterface::SetPosition(start_ + position))
    return false;
  pos_ = position;
  return true;
}

bool StreamSegment::GetPosition(size_t* position) const {
  if (SIZE_UNKNOWN == start_)
    return false;  // Not seekable
  if (!StreamAdapterInterface::GetPosition(position))
    return false;
  if (position) {
    ASSERT(*position >= start_);
    *position -= start_;
  }
  return true;
}

bool StreamSegment::GetSize(size_t* size) const {
  if (!StreamAdapterInterface::GetSize(size))
    return false;
  if (size) {
    if (SIZE_UNKNOWN != start_) {
      ASSERT(*size >= start_);
      *size -= start_;
    }
    if (SIZE_UNKNOWN != length_) {
      *size = _min(*size, length_);
    }
  }
  return true;
}

bool StreamSegment::GetAvailable(size_t* size) const {
  if (!StreamAdapterInterface::GetAvailable(size))
    return false;
  if (size && (SIZE_UNKNOWN != length_))
    *size = _min(*size, length_ - pos_);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// NullStream
///////////////////////////////////////////////////////////////////////////////

NullStream::NullStream() {
}

NullStream::~NullStream() {
}

StreamState NullStream::GetState() const {
  return SS_OPEN;
}

StreamResult NullStream::Read(void* buffer, size_t buffer_len,
                              size_t* read, int* error) {
  if (error) *error = -1;
  return SR_ERROR;
}

StreamResult NullStream::Write(const void* data, size_t data_len,
                               size_t* written, int* error) {
  if (written) *written = data_len;
  return SR_SUCCESS;
}

void NullStream::Close() {
}

///////////////////////////////////////////////////////////////////////////////
// FileStream
///////////////////////////////////////////////////////////////////////////////

FileStream::FileStream() : file_(NULL) {
}

FileStream::~FileStream() {
  FileStream::Close();
}

bool FileStream::Open(const std::string& filename, const char* mode,
                      int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfopen(wfilename.c_str(), ToUtf16(mode).c_str());
  } else {
    if (error) {
      *error = -1;
      return false;
    }
  }
#else
  file_ = fopen(filename.c_str(), mode);
#endif
  if (!file_ && error) {
    *error = errno;
  }
  return (file_ != NULL);
}

bool FileStream::OpenShare(const std::string& filename, const char* mode,
                           int shflag, int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfsopen(wfilename.c_str(), ToUtf16(mode).c_str(), shflag);
    if (!file_ && error) {
      *error = errno;
      return false;
    }
    return file_ != NULL;
  } else {
    if (error) {
      *error = -1;
    }
    return false;
  }
#else
  return Open(filename, mode, error);
#endif
}

bool FileStream::DisableBuffering() {
  if (!file_)
    return false;
  return (setvbuf(file_, NULL, _IONBF, 0) == 0);
}

StreamState FileStream::GetState() const {
  return (file_ == NULL) ? SS_CLOSED : SS_OPEN;
}

StreamResult FileStream::Read(void* buffer, size_t buffer_len,
                              size_t* read, int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fread(buffer, 1, buffer_len, file_);
  if ((result == 0) && (buffer_len > 0)) {
    if (feof(file_))
      return SR_EOS;
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (read)
    *read = result;
  return SR_SUCCESS;
}

StreamResult FileStream::Write(const void* data, size_t data_len,
                               size_t* written, int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fwrite(data, 1, data_len, file_);
  if ((result == 0) && (data_len > 0)) {
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (written)
    *written = result;
  return SR_SUCCESS;
}

void FileStream::Close() {
  if (file_) {
    DoClose();
    file_ = NULL;
  }
}

bool FileStream::SetPosition(size_t position) {
  if (!file_)
    return false;
  return (fseek(file_, static_cast<int>(position), SEEK_SET) == 0);
}

bool FileStream::GetPosition(size_t* position) const {
  ASSERT(NULL != position);
  if (!file_)
    return false;
  long result = ftell(file_);
  if (result < 0)
    return false;
  if (position)
    *position = result;
  return true;
}

bool FileStream::GetSize(size_t* size) const {
  ASSERT(NULL != size);
  if (!file_)
    return false;
  struct stat file_stats;
  if (fstat(fileno(file_), &file_stats) != 0)
    return false;
  if (size)
    *size = file_stats.st_size;
  return true;
}

bool FileStream::GetAvailable(size_t* size) const {
  ASSERT(NULL != size);
  if (!GetSize(size))
    return false;
  long result = ftell(file_);
  if (result < 0)
    return false;
  if (size)
    *size -= result;
  return true;
}

bool FileStream::ReserveSize(size_t size) {
  // TODO: extend the file to the proper length
  return true;
}

bool FileStream::GetSize(const std::string& filename, size_t* size) {
  struct stat file_stats;
  if (stat(filename.c_str(), &file_stats) != 0)
    return false;
  *size = file_stats.st_size;
  return true;
}

bool FileStream::Flush() {
  if (file_) {
    return (0 == fflush(file_));
  }
  // try to flush empty file?
  ASSERT(false);
  return false;
}

#if defined(WEBRTC_POSIX) && !defined(__native_client__)

bool FileStream::TryLock() {
  if (file_ == NULL) {
    // Stream not open.
    ASSERT(false);
    return false;
  }

  return flock(fileno(file_), LOCK_EX|LOCK_NB) == 0;
}

bool FileStream::Unlock() {
  if (file_ == NULL) {
    // Stream not open.
    ASSERT(false);
    return false;
  }

  return flock(fileno(file_), LOCK_UN) == 0;
}

#endif

void FileStream::DoClose() {
  fclose(file_);
}

CircularFileStream::CircularFileStream(size_t max_size)
  : max_write_size_(max_size),
    position_(0),
    marked_position_(max_size / 2),
    last_write_position_(0),
    read_segment_(READ_LATEST),
    read_segment_available_(0) {
}

bool CircularFileStream::Open(
    const std::string& filename, const char* mode, int* error) {
  if (!FileStream::Open(filename.c_str(), mode, error))
    return false;

  if (strchr(mode, "r") != NULL) {  // Opened in read mode.
    // Check if the buffer has been overwritten and determine how to read the
    // log in time sequence.
    size_t file_size;
    GetSize(&file_size);
    if (file_size == position_) {
      // The buffer has not been overwritten yet. Read 0 .. file_size
      read_segment_ = READ_LATEST;
      read_segment_available_ = file_size;
    } else {
      // The buffer has been over written. There are three segments: The first
      // one is 0 .. marked_position_, which is the marked earliest log. The
      // second one is position_ .. file_size, which is the middle log. The
      // last one is marked_position_ .. position_, which is the latest log.
      read_segment_ = READ_MARKED;
      read_segment_available_ = marked_position_;
      last_write_position_ = position_;
    }

    // Read from the beginning.
    position_ = 0;
    SetPosition(position_);
  }

  return true;
}

StreamResult CircularFileStream::Read(void* buffer, size_t buffer_len,
                                      size_t* read, int* error) {
  if (read_segment_available_ == 0) {
    size_t file_size;
    switch (read_segment_) {
      case READ_MARKED:  // Finished READ_MARKED and start READ_MIDDLE.
        read_segment_ = READ_MIDDLE;
        position_ = last_write_position_;
        SetPosition(position_);
        GetSize(&file_size);
        read_segment_available_ = file_size - position_;
        break;

      case READ_MIDDLE:  // Finished READ_MIDDLE and start READ_LATEST.
        read_segment_ = READ_LATEST;
        position_ = marked_position_;
        SetPosition(position_);
        read_segment_available_ = last_write_position_ - position_;
        break;

      default:  // Finished READ_LATEST and return EOS.
        return rtc::SR_EOS;
    }
  }

  size_t local_read;
  if (!read) read = &local_read;

  size_t to_read = rtc::_min(buffer_len, read_segment_available_);
  rtc::StreamResult result
    = rtc::FileStream::Read(buffer, to_read, read, error);
  if (result == rtc::SR_SUCCESS) {
    read_segment_available_ -= *read;
    position_ += *read;
  }
  return result;
}

StreamResult CircularFileStream::Write(const void* data, size_t data_len,
                                       size_t* written, int* error) {
  if (position_ >= max_write_size_) {
    ASSERT(position_ == max_write_size_);
    position_ = marked_position_;
    SetPosition(position_);
  }

  size_t local_written;
  if (!written) written = &local_written;

  size_t to_eof = max_write_size_ - position_;
  size_t to_write = rtc::_min(data_len, to_eof);
  rtc::StreamResult result
    = rtc::FileStream::Write(data, to_write, written, error);
  if (result == rtc::SR_SUCCESS) {
    position_ += *written;
  }
  return result;
}

AsyncWriteStream::~AsyncWriteStream() {
  write_thread_->Clear(this, 0, NULL);
  ClearBufferAndWrite();

  CritScope cs(&crit_stream_);
  stream_.reset();
}

// This is needed by some stream writers, such as RtpDumpWriter.
bool AsyncWriteStream::GetPosition(size_t* position) const {
  CritScope cs(&crit_stream_);
  return stream_->GetPosition(position);
}

// This is needed by some stream writers, such as the plugin log writers.
StreamResult AsyncWriteStream::Read(void* buffer, size_t buffer_len,
                                    size_t* read, int* error) {
  CritScope cs(&crit_stream_);
  return stream_->Read(buffer, buffer_len, read, error);
}

void AsyncWriteStream::Close() {
  if (state_ == SS_CLOSED) {
    return;
  }

  write_thread_->Clear(this, 0, NULL);
  ClearBufferAndWrite();

  CritScope cs(&crit_stream_);
  stream_->Close();
  state_ = SS_CLOSED;
}

StreamResult AsyncWriteStream::Write(const void* data, size_t data_len,
                                     size_t* written, int* error) {
  if (state_ == SS_CLOSED) {
    return SR_ERROR;
  }

  size_t previous_buffer_length = 0;
  {
    CritScope cs(&crit_buffer_);
    previous_buffer_length = buffer_.length();
    buffer_.AppendData(data, data_len);
  }

  if (previous_buffer_length == 0) {
    // If there's stuff already in the buffer, then we already called
    // Post and the write_thread_ hasn't pulled it out yet, so we
    // don't need to re-Post.
    write_thread_->Post(this, 0, NULL);
  }
  // Return immediately, assuming that it works.
  if (written) {
    *written = data_len;
  }
  return SR_SUCCESS;
}

void AsyncWriteStream::OnMessage(rtc::Message* pmsg) {
  ClearBufferAndWrite();
}

bool AsyncWriteStream::Flush() {
  if (state_ == SS_CLOSED) {
    return false;
  }

  ClearBufferAndWrite();

  CritScope cs(&crit_stream_);
  return stream_->Flush();
}

void AsyncWriteStream::ClearBufferAndWrite() {
  Buffer to_write;
  {
    CritScope cs_buffer(&crit_buffer_);
    buffer_.TransferTo(&to_write);
  }

  if (to_write.length() > 0) {
    CritScope cs(&crit_stream_);
    stream_->WriteAll(to_write.data(), to_write.length(), NULL, NULL);
  }
}

#if defined(WEBRTC_POSIX) && !defined(__native_client__)

// Have to identically rewrite the FileStream destructor or else it would call
// the base class's Close() instead of the sub-class's.
POpenStream::~POpenStream() {
  POpenStream::Close();
}

bool POpenStream::Open(const std::string& subcommand,
                       const char* mode,
                       int* error) {
  Close();
  file_ = popen(subcommand.c_str(), mode);
  if (file_ == NULL) {
    if (error)
      *error = errno;
    return false;
  }
  return true;
}

bool POpenStream::OpenShare(const std::string& subcommand, const char* mode,
                            int shflag, int* error) {
  return Open(subcommand, mode, error);
}

void POpenStream::DoClose() {
  wait_status_ = pclose(file_);
}

#endif

///////////////////////////////////////////////////////////////////////////////
// MemoryStream
///////////////////////////////////////////////////////////////////////////////

MemoryStreamBase::MemoryStreamBase()
  : buffer_(NULL), buffer_length_(0), data_length_(0),
    seek_position_(0) {
}

StreamState MemoryStreamBase::GetState() const {
  return SS_OPEN;
}

StreamResult MemoryStreamBase::Read(void* buffer, size_t bytes,
                                    size_t* bytes_read, int* error) {
  if (seek_position_ >= data_length_) {
    return SR_EOS;
  }
  size_t available = data_length_ - seek_position_;
  if (bytes > available) {
    // Read partial buffer
    bytes = available;
  }
  memcpy(buffer, &buffer_[seek_position_], bytes);
  seek_position_ += bytes;
  if (bytes_read) {
    *bytes_read = bytes;
  }
  return SR_SUCCESS;
}

StreamResult MemoryStreamBase::Write(const void* buffer, size_t bytes,
                                     size_t* bytes_written, int* error) {
  size_t available = buffer_length_ - seek_position_;
  if (0 == available) {
    // Increase buffer size to the larger of:
    // a) new position rounded up to next 256 bytes
    // b) double the previous length
    size_t new_buffer_length = _max(((seek_position_ + bytes) | 0xFF) + 1,
                                    buffer_length_ * 2);
    StreamResult result = DoReserve(new_buffer_length, error);
    if (SR_SUCCESS != result) {
      return result;
    }
    ASSERT(buffer_length_ >= new_buffer_length);
    available = buffer_length_ - seek_position_;
  }

  if (bytes > available) {
    bytes = available;
  }
  memcpy(&buffer_[seek_position_], buffer, bytes);
  seek_position_ += bytes;
  if (data_length_ < seek_position_) {
    data_length_ = seek_position_;
  }
  if (bytes_written) {
    *bytes_written = bytes;
  }
  return SR_SUCCESS;
}

void MemoryStreamBase::Close() {
  // nothing to do
}

bool MemoryStreamBase::SetPosition(size_t position) {
  if (position > data_length_)
    return false;
  seek_position_ = position;
  return true;
}

bool MemoryStreamBase::GetPosition(size_t* position) const {
  if (position)
    *position = seek_position_;
  return true;
}

bool MemoryStreamBase::GetSize(size_t* size) const {
  if (size)
    *size = data_length_;
  return true;
}

bool MemoryStreamBase::GetAvailable(size_t* size) const {
  if (size)
    *size = data_length_ - seek_position_;
  return true;
}

bool MemoryStreamBase::ReserveSize(size_t size) {
  return (SR_SUCCESS == DoReserve(size, NULL));
}

StreamResult MemoryStreamBase::DoReserve(size_t size, int* error) {
  return (buffer_length_ >= size) ? SR_SUCCESS : SR_EOS;
}

///////////////////////////////////////////////////////////////////////////////

MemoryStream::MemoryStream()
  : buffer_alloc_(NULL) {
}

MemoryStream::MemoryStream(const char* data)
  : buffer_alloc_(NULL) {
  SetData(data, strlen(data));
}

MemoryStream::MemoryStream(const void* data, size_t length)
  : buffer_alloc_(NULL) {
  SetData(data, length);
}

MemoryStream::~MemoryStream() {
  delete [] buffer_alloc_;
}

void MemoryStream::SetData(const void* data, size_t length) {
  data_length_ = buffer_length_ = length;
  delete [] buffer_alloc_;
  buffer_alloc_ = new char[buffer_length_ + kAlignment];
  buffer_ = reinterpret_cast<char*>(ALIGNP(buffer_alloc_, kAlignment));
  memcpy(buffer_, data, data_length_);
  seek_position_ = 0;
}

StreamResult MemoryStream::DoReserve(size_t size, int* error) {
  if (buffer_length_ >= size)
    return SR_SUCCESS;

  if (char* new_buffer_alloc = new char[size + kAlignment]) {
    char* new_buffer = reinterpret_cast<char*>(
        ALIGNP(new_buffer_alloc, kAlignment));
    memcpy(new_buffer, buffer_, data_length_);
    delete [] buffer_alloc_;
    buffer_alloc_ = new_buffer_alloc;
    buffer_ = new_buffer;
    buffer_length_ = size;
    return SR_SUCCESS;
  }

  if (error) {
    *error = ENOMEM;
  }
  return SR_ERROR;
}

///////////////////////////////////////////////////////////////////////////////

ExternalMemoryStream::ExternalMemoryStream() {
}

ExternalMemoryStream::ExternalMemoryStream(void* data, size_t length) {
  SetData(data, length);
}

ExternalMemoryStream::~ExternalMemoryStream() {
}

void ExternalMemoryStream::SetData(void* data, size_t length) {
  data_length_ = buffer_length_ = length;
  buffer_ = static_cast<char*>(data);
  seek_position_ = 0;
}

///////////////////////////////////////////////////////////////////////////////
// FifoBuffer
///////////////////////////////////////////////////////////////////////////////

FifoBuffer::FifoBuffer(size_t size)
    : state_(SS_OPEN), buffer_(new char[size]), buffer_length_(size),
      data_length_(0), read_position_(0), owner_(Thread::Current()) {
  // all events are done on the owner_ thread
}

FifoBuffer::FifoBuffer(size_t size, Thread* owner)
    : state_(SS_OPEN), buffer_(new char[size]), buffer_length_(size),
      data_length_(0), read_position_(0), owner_(owner) {
  // all events are done on the owner_ thread
}

FifoBuffer::~FifoBuffer() {
}

bool FifoBuffer::GetBuffered(size_t* size) const {
  CritScope cs(&crit_);
  *size = data_length_;
  return true;
}

bool FifoBuffer::SetCapacity(size_t size) {
  CritScope cs(&crit_);
  if (data_length_ > size) {
    return false;
  }

  if (size != buffer_length_) {
    char* buffer = new char[size];
    const size_t copy = data_length_;
    const size_t tail_copy = _min(copy, buffer_length_ - read_position_);
    memcpy(buffer, &buffer_[read_position_], tail_copy);
    memcpy(buffer + tail_copy, &buffer_[0], copy - tail_copy);
    buffer_.reset(buffer);
    read_position_ = 0;
    buffer_length_ = size;
  }
  return true;
}

StreamResult FifoBuffer::ReadOffset(void* buffer, size_t bytes,
                                    size_t offset, size_t* bytes_read) {
  CritScope cs(&crit_);
  return ReadOffsetLocked(buffer, bytes, offset, bytes_read);
}

StreamResult FifoBuffer::WriteOffset(const void* buffer, size_t bytes,
                                     size_t offset, size_t* bytes_written) {
  CritScope cs(&crit_);
  return WriteOffsetLocked(buffer, bytes, offset, bytes_written);
}

StreamState FifoBuffer::GetState() const {
  return state_;
}

StreamResult FifoBuffer::Read(void* buffer, size_t bytes,
                              size_t* bytes_read, int* error) {
  CritScope cs(&crit_);
  const bool was_writable = data_length_ < buffer_length_;
  size_t copy = 0;
  StreamResult result = ReadOffsetLocked(buffer, bytes, 0, &copy);

  if (result == SR_SUCCESS) {
    // If read was successful then adjust the read position and number of
    // bytes buffered.
    read_position_ = (read_position_ + copy) % buffer_length_;
    data_length_ -= copy;
    if (bytes_read) {
      *bytes_read = copy;
    }

    // if we were full before, and now we're not, post an event
    if (!was_writable && copy > 0) {
      PostEvent(owner_, SE_WRITE, 0);
    }
  }
  return result;
}

StreamResult FifoBuffer::Write(const void* buffer, size_t bytes,
                               size_t* bytes_written, int* error) {
  CritScope cs(&crit_);

  const bool was_readable = (data_length_ > 0);
  size_t copy = 0;
  StreamResult result = WriteOffsetLocked(buffer, bytes, 0, &copy);

  if (result == SR_SUCCESS) {
    // If write was successful then adjust the number of readable bytes.
    data_length_ += copy;
    if (bytes_written) {
      *bytes_written = copy;
    }

    // if we didn't have any data to read before, and now we do, post an event
    if (!was_readable && copy > 0) {
      PostEvent(owner_, SE_READ, 0);
    }
  }
  return result;
}

void FifoBuffer::Close() {
  CritScope cs(&crit_);
  state_ = SS_CLOSED;
}

const void* FifoBuffer::GetReadData(size_t* size) {
  CritScope cs(&crit_);
  *size = (read_position_ + data_length_ <= buffer_length_) ?
      data_length_ : buffer_length_ - read_position_;
  return &buffer_[read_position_];
}

void FifoBuffer::ConsumeReadData(size_t size) {
  CritScope cs(&crit_);
  ASSERT(size <= data_length_);
  const bool was_writable = data_length_ < buffer_length_;
  read_position_ = (read_position_ + size) % buffer_length_;
  data_length_ -= size;
  if (!was_writable && size > 0) {
    PostEvent(owner_, SE_WRITE, 0);
  }
}

void* FifoBuffer::GetWriteBuffer(size_t* size) {
  CritScope cs(&crit_);
  if (state_ == SS_CLOSED) {
    return NULL;
  }

  // if empty, reset the write position to the beginning, so we can get
  // the biggest possible block
  if (data_length_ == 0) {
    read_position_ = 0;
  }

  const size_t write_position = (read_position_ + data_length_)
      % buffer_length_;
  *size = (write_position > read_position_ || data_length_ == 0) ?
      buffer_length_ - write_position : read_position_ - write_position;
  return &buffer_[write_position];
}

void FifoBuffer::ConsumeWriteBuffer(size_t size) {
  CritScope cs(&crit_);
  ASSERT(size <= buffer_length_ - data_length_);
  const bool was_readable = (data_length_ > 0);
  data_length_ += size;
  if (!was_readable && size > 0) {
    PostEvent(owner_, SE_READ, 0);
  }
}

bool FifoBuffer::GetWriteRemaining(size_t* size) const {
  CritScope cs(&crit_);
  *size = buffer_length_ - data_length_;
  return true;
}

StreamResult FifoBuffer::ReadOffsetLocked(void* buffer,
                                          size_t bytes,
                                          size_t offset,
                                          size_t* bytes_read) {
  if (offset >= data_length_) {
    return (state_ != SS_CLOSED) ? SR_BLOCK : SR_EOS;
  }

  const size_t available = data_length_ - offset;
  const size_t read_position = (read_position_ + offset) % buffer_length_;
  const size_t copy = _min(bytes, available);
  const size_t tail_copy = _min(copy, buffer_length_ - read_position);
  char* const p = static_cast<char*>(buffer);
  memcpy(p, &buffer_[read_position], tail_copy);
  memcpy(p + tail_copy, &buffer_[0], copy - tail_copy);

  if (bytes_read) {
    *bytes_read = copy;
  }
  return SR_SUCCESS;
}

StreamResult FifoBuffer::WriteOffsetLocked(const void* buffer,
                                           size_t bytes,
                                           size_t offset,
                                           size_t* bytes_written) {
  if (state_ == SS_CLOSED) {
    return SR_EOS;
  }

  if (data_length_ + offset >= buffer_length_) {
    return SR_BLOCK;
  }

  const size_t available = buffer_length_ - data_length_ - offset;
  const size_t write_position = (read_position_ + data_length_ + offset)
      % buffer_length_;
  const size_t copy = _min(bytes, available);
  const size_t tail_copy = _min(copy, buffer_length_ - write_position);
  const char* const p = static_cast<const char*>(buffer);
  memcpy(&buffer_[write_position], p, tail_copy);
  memcpy(&buffer_[0], p + tail_copy, copy - tail_copy);

  if (bytes_written) {
    *bytes_written = copy;
  }
  return SR_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////
// LoggingAdapter
///////////////////////////////////////////////////////////////////////////////

LoggingAdapter::LoggingAdapter(StreamInterface* stream, LoggingSeverity level,
                               const std::string& label, bool hex_mode)
    : StreamAdapterInterface(stream), level_(level), hex_mode_(hex_mode) {
  set_label(label);
}

void LoggingAdapter::set_label(const std::string& label) {
  label_.assign("[");
  label_.append(label);
  label_.append("]");
}

StreamResult LoggingAdapter::Read(void* buffer, size_t buffer_len,
                                  size_t* read, int* error) {
  size_t local_read; if (!read) read = &local_read;
  StreamResult result = StreamAdapterInterface::Read(buffer, buffer_len, read,
                                                     error);
  if (result == SR_SUCCESS) {
    LogMultiline(level_, label_.c_str(), true, buffer, *read, hex_mode_, &lms_);
  }
  return result;
}

StreamResult LoggingAdapter::Write(const void* data, size_t data_len,
                                   size_t* written, int* error) {
  size_t local_written;
  if (!written) written = &local_written;
  StreamResult result = StreamAdapterInterface::Write(data, data_len, written,
                                                      error);
  if (result == SR_SUCCESS) {
    LogMultiline(level_, label_.c_str(), false, data, *written, hex_mode_,
                 &lms_);
  }
  return result;
}

void LoggingAdapter::Close() {
  LogMultiline(level_, label_.c_str(), false, NULL, 0, hex_mode_, &lms_);
  LogMultiline(level_, label_.c_str(), true, NULL, 0, hex_mode_, &lms_);
  LOG_V(level_) << label_ << " Closed locally";
  StreamAdapterInterface::Close();
}

void LoggingAdapter::OnEvent(StreamInterface* stream, int events, int err) {
  if (events & SE_OPEN) {
    LOG_V(level_) << label_ << " Open";
  } else if (events & SE_CLOSE) {
    LogMultiline(level_, label_.c_str(), false, NULL, 0, hex_mode_, &lms_);
    LogMultiline(level_, label_.c_str(), true, NULL, 0, hex_mode_, &lms_);
    LOG_V(level_) << label_ << " Closed with error: " << err;
  }
  StreamAdapterInterface::OnEvent(stream, events, err);
}

///////////////////////////////////////////////////////////////////////////////
// StringStream - Reads/Writes to an external std::string
///////////////////////////////////////////////////////////////////////////////

StringStream::StringStream(std::string& str)
    : str_(str), read_pos_(0), read_only_(false) {
}

StringStream::StringStream(const std::string& str)
    : str_(const_cast<std::string&>(str)), read_pos_(0), read_only_(true) {
}

StreamState StringStream::GetState() const {
  return SS_OPEN;
}

StreamResult StringStream::Read(void* buffer, size_t buffer_len,
                                      size_t* read, int* error) {
  size_t available = _min(buffer_len, str_.size() - read_pos_);
  if (!available)
    return SR_EOS;
  memcpy(buffer, str_.data() + read_pos_, available);
  read_pos_ += available;
  if (read)
    *read = available;
  return SR_SUCCESS;
}

StreamResult StringStream::Write(const void* data, size_t data_len,
                                      size_t* written, int* error) {
  if (read_only_) {
    if (error) {
      *error = -1;
    }
    return SR_ERROR;
  }
  str_.append(static_cast<const char*>(data),
              static_cast<const char*>(data) + data_len);
  if (written)
    *written = data_len;
  return SR_SUCCESS;
}

void StringStream::Close() {
}

bool StringStream::SetPosition(size_t position) {
  if (position > str_.size())
    return false;
  read_pos_ = position;
  return true;
}

bool StringStream::GetPosition(size_t* position) const {
  if (position)
    *position = read_pos_;
  return true;
}

bool StringStream::GetSize(size_t* size) const {
  if (size)
    *size = str_.size();
  return true;
}

bool StringStream::GetAvailable(size_t* size) const {
  if (size)
    *size = str_.size() - read_pos_;
  return true;
}

bool StringStream::ReserveSize(size_t size) {
  if (read_only_)
    return false;
  str_.reserve(size);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// StreamReference
///////////////////////////////////////////////////////////////////////////////

StreamReference::StreamReference(StreamInterface* stream)
    : StreamAdapterInterface(stream, false) {
  // owner set to false so the destructor does not free the stream.
  stream_ref_count_ = new StreamRefCount(stream);
}

StreamInterface* StreamReference::NewReference() {
  stream_ref_count_->AddReference();
  return new StreamReference(stream_ref_count_, stream());
}

StreamReference::~StreamReference() {
  stream_ref_count_->Release();
}

StreamReference::StreamReference(StreamRefCount* stream_ref_count,
                                 StreamInterface* stream)
    : StreamAdapterInterface(stream, false),
      stream_ref_count_(stream_ref_count) {
}

///////////////////////////////////////////////////////////////////////////////

StreamResult Flow(StreamInterface* source,
                  char* buffer, size_t buffer_len,
                  StreamInterface* sink,
                  size_t* data_len /* = NULL */) {
  ASSERT(buffer_len > 0);

  StreamResult result;
  size_t count, read_pos, write_pos;
  if (data_len) {
    read_pos = *data_len;
  } else {
    read_pos = 0;
  }

  bool end_of_stream = false;
  do {
    // Read until buffer is full, end of stream, or error
    while (!end_of_stream && (read_pos < buffer_len)) {
      result = source->Read(buffer + read_pos, buffer_len - read_pos,
                            &count, NULL);
      if (result == SR_EOS) {
        end_of_stream = true;
      } else if (result != SR_SUCCESS) {
        if (data_len) {
          *data_len = read_pos;
        }
        return result;
      } else {
        read_pos += count;
      }
    }

    // Write until buffer is empty, or error (including end of stream)
    write_pos = 0;
    while (write_pos < read_pos) {
      result = sink->Write(buffer + write_pos, read_pos - write_pos,
                           &count, NULL);
      if (result != SR_SUCCESS) {
        if (data_len) {
          *data_len = read_pos - write_pos;
          if (write_pos > 0) {
            memmove(buffer, buffer + write_pos, *data_len);
          }
        }
        return result;
      }
      write_pos += count;
    }

    read_pos = 0;
  } while (!end_of_stream);

  if (data_len) {
    *data_len = 0;
  }
  return SR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
