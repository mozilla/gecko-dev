/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/rtp_file_writer.h"

#include <stdio.h>

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {
namespace test {

static const uint16_t kPacketHeaderSize = 8;
static const char kFirstLine[] = "#!rtpplay1.0 0.0.0.0/0\n";

// Write RTP packets to file in rtpdump format, as documented at:
// http://www.cs.columbia.edu/irt/software/rtptools/
class RtpDumpWriter : public RtpFileWriter {
 public:
  explicit RtpDumpWriter(FILE* file) : file_(file) {
    CHECK(file_ != NULL);
    Init();
  }
  virtual ~RtpDumpWriter() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  bool WritePacket(const RtpPacket* packet) override {
    uint16_t len = static_cast<uint16_t>(packet->length + kPacketHeaderSize);
    CHECK_GE(packet->original_length, packet->length);
    uint16_t plen = static_cast<uint16_t>(packet->original_length);
    uint32_t offset = packet->time_ms;
    CHECK(WriteUint16(len));
    CHECK(WriteUint16(plen));
    CHECK(WriteUint32(offset));
    return fwrite(packet->data, sizeof(uint8_t), packet->length, file_) ==
           packet->length;
  }

 private:
  bool Init() {
    fprintf(file_, "%s", kFirstLine);

    CHECK(WriteUint32(0));
    CHECK(WriteUint32(0));
    CHECK(WriteUint32(0));
    CHECK(WriteUint16(0));
    CHECK(WriteUint16(0));

    return true;
  }

  bool WriteUint32(uint32_t in) {
    // Loop through shifts = {24, 16, 8, 0}.
    for (int shifts = 24; shifts >= 0; shifts -= 8) {
      uint8_t tmp = static_cast<uint8_t>((in >> shifts) & 0xFF);
      if (fwrite(&tmp, sizeof(uint8_t), 1, file_) != 1)
        return false;
    }
    return true;
  }

  bool WriteUint16(uint16_t in) {
    // Write 8 MSBs.
    uint8_t tmp = static_cast<uint8_t>((in >> 8) & 0xFF);
    if (fwrite(&tmp, sizeof(uint8_t), 1, file_) != 1)
      return false;
    // Write 8 LSBs.
    tmp = static_cast<uint8_t>(in & 0xFF);
    if (fwrite(&tmp, sizeof(uint8_t), 1, file_) != 1)
      return false;
    return true;
  }

  FILE* file_;

  DISALLOW_COPY_AND_ASSIGN(RtpDumpWriter);
};

RtpFileWriter* RtpFileWriter::Create(FileFormat format,
                                     const std::string& filename) {
  FILE* file = fopen(filename.c_str(), "wb");
  if (file == NULL) {
    printf("ERROR: Can't open file: %s\n", filename.c_str());
    return NULL;
  }
  switch (format) {
    case kRtpDump:
      return new RtpDumpWriter(file);
  }
  fclose(file);
  return NULL;
}

}  // namespace test
}  // namespace webrtc
