/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).
// See header file for details and examples.

#include "base/histogram.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "base/logging.h"

namespace base {

#define DVLOG(x) CHROMIUM_LOG(ERROR)
#define CHECK_GT DCHECK_GT
#define CHECK_LT DCHECK_LT
typedef ::Lock Lock;
typedef ::AutoLock AutoLock;

// Static table of checksums for all possible 8 bit bytes.
const uint32_t Histogram::kCrcTable[256] = {0x0, 0x77073096L, 0xee0e612cL,
0x990951baL, 0x76dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0xedb8832L,
0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L, 0x9b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL, 0x1adad47dL,
0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L, 0x646ba8c0L, 0xfd62f97aL,
0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L,
0x4c69105eL, 0xd56041e4L, 0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL,
0xa50ab56bL, 0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL, 0xc8d75180L,
0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL, 0x2802b89eL,
0x5f058808L, 0xc60cd9b2L, 0xb10be924L, 0x2f6f7c87L, 0x58684c11L, 0xc1611dabL,
0xb6662d3dL, 0x76dc4190L, 0x1db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L,
0x6b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0xf00f934L, 0x9609a88eL,
0xe10e9818L, 0x7f6a0dbbL, 0x86d3d2dL, 0x91646c97L, 0xe6635c01L, 0x6b6b51f4L,
0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L,
0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL,
0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L,
0xd4bb30e2L, 0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L, 0xaa0a4c5fL,
0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L, 0x5768b525L,
0x206f85b3L, 0xb966d409L, 0xce61e49fL, 0x5edef90eL, 0x29d9c998L, 0xb0d09822L,
0xc7d7a8b4L, 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L,
0x9abfb3b6L, 0x3b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x4db2615L,
0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0xd6d6a3eL, 0x7a6a5aa8L, 0xe40ecf0bL,
0x9309ff9dL, 0xa00ae27L, 0x7d079eb1L, 0xf00f9344L, 0x8708a3d2L, 0x1e01f268L,
0x6906c2feL, 0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L, 0xfed41b76L,
0x89d32be0L, 0x10da7a5aL, 0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L,
0x60b08ed5L, 0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L,
0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L, 0xcb61b38cL,
0xbc66831aL, 0x256fd2a0L, 0x5268e236L, 0xcc0c7795L, 0xbb0b4703L, 0x220216b9L,
0x5505262fL, 0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L,
0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
0x26d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x5005713L, 0x95bf4a82L,
0xe2b87a14L, 0x7bb12baeL, 0xcb61b38L, 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L,
0xbdbdf21L, 0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL,
0xf6b9265bL, 0x6fb077e1L, 0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL,
0x11010b5cL, 0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L, 0x4969474dL,
0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L, 0xa9bcae53L,
0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L, 0xbdbdf21cL, 0xcabac28aL, 0x53b39330L,
0x24b4a3a6L, 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL,
0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
0x2d02ef8dL,
};

typedef Histogram::Count Count;

// static
const size_t Histogram::kBucketCount_MAX = 16384u;

Histogram* Histogram::FactoryGet(const std::string& name,
                                 Sample minimum,
                                 Sample maximum,
                                 size_t bucket_count,
                                 Flags flags) {
  Histogram* histogram(NULL);

  // Defensive code.
  if (minimum < 1)
    minimum = 1;
  if (maximum > kSampleType_MAX - 1)
    maximum = kSampleType_MAX - 1;

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    // Extra variable is not needed... but this keeps this section basically
    // identical to other derived classes in this file (and compiler will
    // optimize away the extra variable.
    Histogram* tentative_histogram =
        new Histogram(name, minimum, maximum, bucket_count);
    tentative_histogram->InitializeBucketRange();
    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }

  DCHECK_EQ(HISTOGRAM, histogram->histogram_type());
  DCHECK(histogram->HasConstructorArguments(minimum, maximum, bucket_count));
  return histogram;
}

Histogram* Histogram::FactoryTimeGet(const std::string& name,
                                     TimeDelta minimum,
                                     TimeDelta maximum,
                                     size_t bucket_count,
                                     Flags flags) {
  return FactoryGet(name, minimum.InMilliseconds(), maximum.InMilliseconds(),
                    bucket_count, flags);
}

void Histogram::Add(int value) {
  if (value > kSampleType_MAX - 1)
    value = kSampleType_MAX - 1;
  if (value < 0)
    value = 0;
  size_t index = BucketIndex(value);
  DCHECK_GE(value, ranges(index));
  DCHECK_LT(value, ranges(index + 1));
  Accumulate(value, 1, index);
}

void Histogram::Subtract(int value) {
  if (value > kSampleType_MAX - 1)
    value = kSampleType_MAX - 1;
  if (value < 0)
    value = 0;
  size_t index = BucketIndex(value);
  DCHECK_GE(value, ranges(index));
  DCHECK_LT(value, ranges(index + 1));
  Accumulate(value, -1, index);
}

void Histogram::AddBoolean(bool value) {
  DCHECK(false);
}

void Histogram::AddSampleSet(const SampleSet& sample) {
  sample_.Add(sample);
}

void Histogram::Clear() {
  SampleSet ss;
  ss.Resize(*this);
  sample_ = ss;
}

void Histogram::SetRangeDescriptions(const DescriptionPair descriptions[]) {
  DCHECK(false);
}

// The following methods provide a graphical histogram display.
void Histogram::WriteHTMLGraph(std::string* output) const {
  // TBD(jar) Write a nice HTML bar chart, with divs an mouse-overs etc.
  output->append("<PRE>");
  WriteAscii(true, "<br>", output);
  output->append("</PRE>");
}

void Histogram::WriteAscii(bool graph_it, const std::string& newline,
                           std::string* output) const {
  // Get local (stack) copies of all effectively volatile class data so that we
  // are consistent across our output activities.
  SampleSet snapshot;
  SnapshotSample(&snapshot);

  Count sample_count = snapshot.TotalCount();

  WriteAsciiHeader(snapshot, sample_count, output);
  output->append(newline);

  // Prepare to normalize graphical rendering of bucket contents.
  double max_size = 0;
  if (graph_it)
    max_size = GetPeakBucketSize(snapshot);

  // Calculate space needed to print bucket range numbers.  Leave room to print
  // nearly the largest bucket range without sliding over the histogram.
  size_t largest_non_empty_bucket = bucket_count() - 1;
  while (0 == snapshot.counts(largest_non_empty_bucket)) {
    if (0 == largest_non_empty_bucket)
      break;  // All buckets are empty.
    --largest_non_empty_bucket;
  }

  // Calculate largest print width needed for any of our bucket range displays.
  size_t print_width = 1;
  for (size_t i = 0; i < bucket_count(); ++i) {
    if (snapshot.counts(i)) {
      size_t width = GetAsciiBucketRange(i).size() + 1;
      if (width > print_width)
        print_width = width;
    }
  }

  int64_t remaining = sample_count;
  int64_t past = 0;
  // Output the actual histogram graph.
  for (size_t i = 0; i < bucket_count(); ++i) {
    Count current = snapshot.counts(i);
    if (!current && !PrintEmptyBucket(i))
      continue;
    remaining -= current;
    std::string range = GetAsciiBucketRange(i);
    output->append(range);
    for (size_t j = 0; range.size() + j < print_width + 1; ++j)
      output->push_back(' ');
    if (0 == current &&
        i < bucket_count() - 1 && 0 == snapshot.counts(i + 1)) {
      while (i < bucket_count() - 1 && 0 == snapshot.counts(i + 1))
        ++i;
      output->append("... ");
      output->append(newline);
      continue;  // No reason to plot emptiness.
    }
    double current_size = GetBucketSize(current, i);
    if (graph_it)
      WriteAsciiBucketGraph(current_size, max_size, output);
    WriteAsciiBucketContext(past, current, remaining, i, output);
    output->append(newline);
    past += current;
  }
  DCHECK_EQ(sample_count, past);
}

//------------------------------------------------------------------------------
// Methods for the validating a sample and a related histogram.
//------------------------------------------------------------------------------

Histogram::Inconsistencies
Histogram::FindCorruption(const SampleSet& snapshot) const
{
  int inconsistencies = NO_INCONSISTENCIES;
  Sample previous_range = -1;  // Bottom range is always 0.
  int64_t count = 0;
  for (size_t index = 0; index < bucket_count(); ++index) {
    count += snapshot.counts(index);
    int new_range = ranges(index);
    if (previous_range >= new_range)
      inconsistencies |= BUCKET_ORDER_ERROR;
    previous_range = new_range;
  }

  if (!HasValidRangeChecksum())
    inconsistencies |= RANGE_CHECKSUM_ERROR;

  int64_t delta64 = snapshot.redundant_count() - count;
  if (delta64 != 0) {
    int delta = static_cast<int>(delta64);
    if (delta != delta64)
      delta = INT_MAX;  // Flag all giant errors as INT_MAX.
    // Since snapshots of histograms are taken asynchronously relative to
    // sampling (and snapped from different threads), it is pretty likely that
    // we'll catch a redundant count that doesn't match the sample count.  We
    // allow for a certain amount of slop before flagging this as an
    // inconsistency.  Even with an inconsistency, we'll snapshot it again (for
    // UMA in about a half hour, so we'll eventually get the data, if it was
    // not the result of a corruption.  If histograms show that 1 is "too tight"
    // then we may try to use 2 or 3 for this slop value.
    const int kCommonRaceBasedCountMismatch = 1;
    if (delta > 0) {
      UMA_HISTOGRAM_COUNTS("Histogram.InconsistentCountHigh", delta);
      if (delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_HIGH_ERROR;
    } else {
      DCHECK_GT(0, delta);
      UMA_HISTOGRAM_COUNTS("Histogram.InconsistentCountLow", -delta);
      if (-delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_LOW_ERROR;
    }
  }
  return static_cast<Inconsistencies>(inconsistencies);
}

Histogram::ClassType Histogram::histogram_type() const {
  return HISTOGRAM;
}

Histogram::Sample Histogram::ranges(size_t i) const {
  return ranges_[i];
}

size_t Histogram::bucket_count() const {
  return bucket_count_;
}

void Histogram::SnapshotSample(SampleSet* sample) const {
  *sample = sample_;
}

bool Histogram::HasConstructorArguments(Sample minimum,
                                        Sample maximum,
                                        size_t bucket_count) {
  return ((minimum == declared_min_) && (maximum == declared_max_) &&
          (bucket_count == bucket_count_));
}

bool Histogram::HasConstructorTimeDeltaArguments(TimeDelta minimum,
                                                 TimeDelta maximum,
                                                 size_t bucket_count) {
  return ((minimum.InMilliseconds() == declared_min_) &&
          (maximum.InMilliseconds() == declared_max_) &&
          (bucket_count == bucket_count_));
}

bool Histogram::HasValidRangeChecksum() const {
  return CalculateRangeChecksum() == range_checksum_;
}

size_t Histogram::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = 0;
  n += aMallocSizeOf(this);
  // We're not allowed to do deep dives into STL data structures.  This
  // is as close as we can get to measuring this array.
  n += aMallocSizeOf(&ranges_[0]);
  n += sample_.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

size_t
Histogram::SampleSet::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  // We're not allowed to do deep dives into STL data structures.  This
  // is as close as we can get to measuring this array.
  return aMallocSizeOf(&counts_[0]);
}

Histogram::Histogram(const std::string& name, Sample minimum,
                     Sample maximum, size_t bucket_count)
  : sample_(),
    histogram_name_(name),
    declared_min_(minimum),
    declared_max_(maximum),
    bucket_count_(bucket_count),
    flags_(kNoFlags),
    ranges_(bucket_count + 1, 0),
    range_checksum_(0),
    recording_enabled_(true) {
  Initialize();
}

Histogram::Histogram(const std::string& name, TimeDelta minimum,
                     TimeDelta maximum, size_t bucket_count)
  : sample_(),
    histogram_name_(name),
    declared_min_(static_cast<int> (minimum.InMilliseconds())),
    declared_max_(static_cast<int> (maximum.InMilliseconds())),
    bucket_count_(bucket_count),
    flags_(kNoFlags),
    ranges_(bucket_count + 1, 0),
    range_checksum_(0),
    recording_enabled_(true) {
  Initialize();
}

Histogram::~Histogram() {
  if (StatisticsRecorder::dump_on_exit()) {
    std::string output;
    WriteAscii(true, "\n", &output);
    CHROMIUM_LOG(INFO) << output;
  }

  // Just to make sure most derived class did this properly...
  DCHECK(ValidateBucketRanges());
}

// Calculate what range of values are held in each bucket.
// We have to be careful that we don't pick a ratio between starting points in
// consecutive buckets that is sooo small, that the integer bounds are the same
// (effectively making one bucket get no values).  We need to avoid:
//   ranges_[i] == ranges_[i + 1]
// To avoid that, we just do a fine-grained bucket width as far as we need to
// until we get a ratio that moves us along at least 2 units at a time.  From
// that bucket onward we do use the exponential growth of buckets.
void Histogram::InitializeBucketRange() {
  double log_max = log(static_cast<double>(declared_max()));
  double log_ratio;
  double log_next;
  size_t bucket_index = 1;
  Sample current = declared_min();
  SetBucketRange(bucket_index, current);
  while (bucket_count() > ++bucket_index) {
    double log_current;
    log_current = log(static_cast<double>(current));
    // Calculate the count'th root of the range.
    log_ratio = (log_max - log_current) / (bucket_count() - bucket_index);
    // See where the next bucket would start.
    log_next = log_current + log_ratio;
    int next;
    next = static_cast<int>(floor(exp(log_next) + 0.5));
    if (next > current)
      current = next;
    else
      ++current;  // Just do a narrow bucket, and keep trying.
    SetBucketRange(bucket_index, current);
  }
  ResetRangeChecksum();

  DCHECK_EQ(bucket_count(), bucket_index);
}

bool Histogram::PrintEmptyBucket(size_t index) const {
  return true;
}

size_t Histogram::BucketIndex(Sample value) const {
  // Use simple binary search.  This is very general, but there are better
  // approaches if we knew that the buckets were linearly distributed.
  DCHECK_LE(ranges(0), value);
  DCHECK_GT(ranges(bucket_count()), value);
  size_t under = 0;
  size_t over = bucket_count();
  size_t mid;

  do {
    DCHECK_GE(over, under);
    mid = under + (over - under)/2;
    if (mid == under)
      break;
    if (ranges(mid) <= value)
      under = mid;
    else
      over = mid;
  } while (true);

  DCHECK_LE(ranges(mid), value);
  CHECK_GT(ranges(mid+1), value);
  return mid;
}

// Use the actual bucket widths (like a linear histogram) until the widths get
// over some transition value, and then use that transition width.  Exponentials
// get so big so fast (and we don't expect to see a lot of entries in the large
// buckets), so we need this to make it possible to see what is going on and
// not have 0-graphical-height buckets.
double Histogram::GetBucketSize(Count current, size_t i) const {
  DCHECK_GT(ranges(i + 1), ranges(i));
  static const double kTransitionWidth = 5;
  double denominator = ranges(i + 1) - ranges(i);
  if (denominator > kTransitionWidth)
    denominator = kTransitionWidth;  // Stop trying to normalize.
  return current/denominator;
}

void Histogram::ResetRangeChecksum() {
  range_checksum_ = CalculateRangeChecksum();
}

const std::string Histogram::GetAsciiBucketRange(size_t i) const {
  std::string result;
  if (kHexRangePrintingFlag & flags_)
    StringAppendF(&result, "%#x", ranges(i));
  else
    StringAppendF(&result, "%d", ranges(i));
  return result;
}

// Update histogram data with new sample.
void Histogram::Accumulate(Sample value, Count count, size_t index) {
  sample_.Accumulate(value, count, index);
}

void Histogram::SetBucketRange(size_t i, Sample value) {
  DCHECK_GT(bucket_count_, i);
  ranges_[i] = value;
}

bool Histogram::ValidateBucketRanges() const {
  // Standard assertions that all bucket ranges should satisfy.
  DCHECK_EQ(bucket_count_ + 1, ranges_.size());
  DCHECK_EQ(0, ranges_[0]);
  DCHECK_EQ(declared_min(), ranges_[1]);
  DCHECK_EQ(declared_max(), ranges_[bucket_count_ - 1]);
  DCHECK_EQ(kSampleType_MAX, ranges_[bucket_count_]);
  return true;
}

uint32_t Histogram::CalculateRangeChecksum() const {
  DCHECK_EQ(ranges_.size(), bucket_count() + 1);
  uint32_t checksum = static_cast<uint32_t>(ranges_.size());  // Seed checksum.
  for (size_t index = 0; index < bucket_count(); ++index)
    checksum = Crc32(checksum, ranges(index));
  return checksum;
}

void Histogram::Initialize() {
  sample_.Resize(*this);
  if (declared_min_ < 1)
    declared_min_ = 1;
  if (declared_max_ > kSampleType_MAX - 1)
    declared_max_ = kSampleType_MAX - 1;
  DCHECK_LE(declared_min_, declared_max_);
  DCHECK_GT(bucket_count_, 1u);
  CHECK_LT(bucket_count_, kBucketCount_MAX);
  size_t maximal_bucket_count = declared_max_ - declared_min_ + 2;
  DCHECK_LE(bucket_count_, maximal_bucket_count);
  DCHECK_EQ(0, ranges_[0]);
  ranges_[bucket_count_] = kSampleType_MAX;
}

// We generate the CRC-32 using the low order bits to select whether to XOR in
// the reversed polynomial 0xedb88320L.  This is nice and simple, and allows us
// to keep the quotient in a uint32_t.  Since we're not concerned about the nature
// of corruptions (i.e., we don't care about bit sequencing, since we are
// handling memory changes, which are more grotesque) so we don't bother to
// get the CRC correct for big-endian vs little-ending calculations.  All we
// need is a nice hash, that tends to depend on all the bits of the sample, with
// very little chance of changes in one place impacting changes in another
// place.
uint32_t Histogram::Crc32(uint32_t sum, Histogram::Sample range) {
  const bool kUseRealCrc = true;  // TODO(jar): Switch to false and watch stats.
  if (kUseRealCrc) {
    union {
      Histogram::Sample range;
      unsigned char bytes[sizeof(Histogram::Sample)];
    } converter;
    converter.range = range;
    for (size_t i = 0; i < sizeof(converter); ++i)
      sum = kCrcTable[(sum & 0xff) ^ converter.bytes[i]] ^ (sum >> 8);
  } else {
    // Use hash techniques provided in ReallyFastHash, except we don't care
    // about "avalanching" (which would worsten the hash, and add collisions),
    // and we don't care about edge cases since we have an even number of bytes.
    union {
      Histogram::Sample range;
      uint16_t ints[sizeof(Histogram::Sample) / 2];
    } converter;
    DCHECK_EQ(sizeof(Histogram::Sample), sizeof(converter));
    converter.range = range;
    sum += converter.ints[0];
    sum = (sum << 16) ^ sum ^ (static_cast<uint32_t>(converter.ints[1]) << 11);
    sum += sum >> 11;
  }
  return sum;
}

//------------------------------------------------------------------------------
// Private methods

double Histogram::GetPeakBucketSize(const SampleSet& snapshot) const {
  double max = 0;
  for (size_t i = 0; i < bucket_count() ; ++i) {
    double current_size
        = GetBucketSize(snapshot.counts(i), i);
    if (current_size > max)
      max = current_size;
  }
  return max;
}

void Histogram::WriteAsciiHeader(const SampleSet& snapshot,
                                 Count sample_count,
                                 std::string* output) const {
  StringAppendF(output,
                "Histogram: %s recorded %d samples",
                histogram_name().c_str(),
                sample_count);
  int64_t snapshot_sum = snapshot.sum();
  if (0 == sample_count) {
    DCHECK_EQ(snapshot_sum, 0);
  } else {
    double average = static_cast<float>(snapshot_sum) / sample_count;

    StringAppendF(output, ", average = %.1f", average);
  }
  if (flags_ & ~kHexRangePrintingFlag)
    StringAppendF(output, " (flags = 0x%x)", flags_ & ~kHexRangePrintingFlag);
}

void Histogram::WriteAsciiBucketContext(const int64_t past,
                                        const Count current,
                                        const int64_t remaining,
                                        const size_t i,
                                        std::string* output) const {
  double scaled_sum = (past + current + remaining) / 100.0;
  WriteAsciiBucketValue(current, scaled_sum, output);
  if (0 < i) {
    double percentage = past / scaled_sum;
    StringAppendF(output, " {%3.1f%%}", percentage);
  }
}

void Histogram::WriteAsciiBucketValue(Count current, double scaled_sum,
                                      std::string* output) const {
  StringAppendF(output, " (%d = %3.1f%%)", current, current/scaled_sum);
}

void Histogram::WriteAsciiBucketGraph(double current_size, double max_size,
                                      std::string* output) const {
  const int k_line_length = 72;  // Maximal horizontal width of graph.
  int x_count = static_cast<int>(k_line_length * (current_size / max_size)
                                 + 0.5);
  int x_remainder = k_line_length - x_count;

  while (0 < x_count--)
    output->append("-");
  output->append("O");
  while (0 < x_remainder--)
    output->append(" ");
}

//------------------------------------------------------------------------------
// Methods for the Histogram::SampleSet class
//------------------------------------------------------------------------------

Histogram::SampleSet::SampleSet()
    : counts_(),
      sum_(0),
      redundant_count_(0) {
}

Histogram::SampleSet::~SampleSet() {
}

void Histogram::SampleSet::Resize(const Histogram& histogram) {
  counts_.resize(histogram.bucket_count(), 0);
}

void Histogram::SampleSet::Accumulate(Sample value, Count count,
                                      size_t index) {
  DCHECK(count == 1 || count == -1);
  counts_[index] += count;
  redundant_count_ += count;
  sum_ += static_cast<int64_t>(count) * value;
  DCHECK_GE(counts_[index], 0);
  DCHECK_GE(sum_, 0);
  DCHECK_GE(redundant_count_, 0);
}

Count Histogram::SampleSet::TotalCount() const {
  Count total = 0;
  for (Counts::const_iterator it = counts_.begin();
       it != counts_.end();
       ++it) {
    total += *it;
  }
  return total;
}

void Histogram::SampleSet::Add(const SampleSet& other) {
  DCHECK_EQ(counts_.size(), other.counts_.size());
  sum_ += other.sum_;
  redundant_count_ += other.redundant_count_;
  for (size_t index = 0; index < counts_.size(); ++index)
    counts_[index] += other.counts_[index];
}

//------------------------------------------------------------------------------
// LinearHistogram: This histogram uses a traditional set of evenly spaced
// buckets.
//------------------------------------------------------------------------------

LinearHistogram::~LinearHistogram() {
}

Histogram* LinearHistogram::FactoryGet(const std::string& name,
                                       Sample minimum,
                                       Sample maximum,
                                       size_t bucket_count,
                                       Flags flags) {
  Histogram* histogram(NULL);

  if (minimum < 1)
    minimum = 1;
  if (maximum > kSampleType_MAX - 1)
    maximum = kSampleType_MAX - 1;

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    LinearHistogram* tentative_histogram =
        new LinearHistogram(name, minimum, maximum, bucket_count);
    tentative_histogram->InitializeBucketRange();
    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }

  DCHECK_EQ(LINEAR_HISTOGRAM, histogram->histogram_type());
  DCHECK(histogram->HasConstructorArguments(minimum, maximum, bucket_count));
  return histogram;
}

Histogram* LinearHistogram::FactoryTimeGet(const std::string& name,
                                           TimeDelta minimum,
                                           TimeDelta maximum,
                                           size_t bucket_count,
                                           Flags flags) {
  return FactoryGet(name, minimum.InMilliseconds(), maximum.InMilliseconds(),
                    bucket_count, flags);
}

Histogram::ClassType LinearHistogram::histogram_type() const {
  return LINEAR_HISTOGRAM;
}

void LinearHistogram::Accumulate(Sample value, Count count, size_t index) {
  sample_.Accumulate(value, count, index);
}

void LinearHistogram::SetRangeDescriptions(
    const DescriptionPair descriptions[]) {
  for (int i =0; descriptions[i].description; ++i) {
    bucket_description_[descriptions[i].sample] = descriptions[i].description;
  }
}

LinearHistogram::LinearHistogram(const std::string& name,
                                 Sample minimum,
                                 Sample maximum,
                                 size_t bucket_count)
    : Histogram(name, minimum >= 1 ? minimum : 1, maximum, bucket_count) {
}

LinearHistogram::LinearHistogram(const std::string& name,
                                 TimeDelta minimum,
                                 TimeDelta maximum,
                                 size_t bucket_count)
    : Histogram(name, minimum >= TimeDelta::FromMilliseconds(1) ?
                                 minimum : TimeDelta::FromMilliseconds(1),
                maximum, bucket_count) {
}

void LinearHistogram::InitializeBucketRange() {
  DCHECK_GT(declared_min(), 0);  // 0 is the underflow bucket here.
  double min = declared_min();
  double max = declared_max();
  size_t i;
  for (i = 1; i < bucket_count(); ++i) {
    double linear_range = (min * (bucket_count() -1 - i) + max * (i - 1)) /
                          (bucket_count() - 2);
    SetBucketRange(i, static_cast<int> (linear_range + 0.5));
  }
  ResetRangeChecksum();
}

double LinearHistogram::GetBucketSize(Count current, size_t i) const {
  DCHECK_GT(ranges(i + 1), ranges(i));
  // Adjacent buckets with different widths would have "surprisingly" many (few)
  // samples in a histogram if we didn't normalize this way.
  double denominator = ranges(i + 1) - ranges(i);
  return current/denominator;
}

const std::string LinearHistogram::GetAsciiBucketRange(size_t i) const {
  int range = ranges(i);
  BucketDescriptionMap::const_iterator it = bucket_description_.find(range);
  if (it == bucket_description_.end())
    return Histogram::GetAsciiBucketRange(i);
  return it->second;
}

bool LinearHistogram::PrintEmptyBucket(size_t index) const {
  return bucket_description_.find(ranges(index)) == bucket_description_.end();
}


//------------------------------------------------------------------------------
// This section provides implementation for BooleanHistogram.
//------------------------------------------------------------------------------

Histogram* BooleanHistogram::FactoryGet(const std::string& name, Flags flags) {
  Histogram* histogram(NULL);

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    BooleanHistogram* tentative_histogram = new BooleanHistogram(name);
    tentative_histogram->InitializeBucketRange();
    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }

  DCHECK_EQ(BOOLEAN_HISTOGRAM, histogram->histogram_type());
  return histogram;
}

Histogram::ClassType BooleanHistogram::histogram_type() const {
  return BOOLEAN_HISTOGRAM;
}

void BooleanHistogram::AddBoolean(bool value) {
  Add(value ? 1 : 0);
}

BooleanHistogram::BooleanHistogram(const std::string& name)
    : LinearHistogram(name, 1, 2, 3) {
}

void
BooleanHistogram::Accumulate(Sample value, Count count, size_t index)
{
  // Callers will have computed index based on the non-booleanified value.
  // So we need to adjust the index manually.
  LinearHistogram::Accumulate(!!value, count, value ? 1 : 0);
}

//------------------------------------------------------------------------------
// FlagHistogram:
//------------------------------------------------------------------------------

Histogram *
FlagHistogram::FactoryGet(const std::string &name, Flags flags)
{
  Histogram *h(nullptr);

  if (!StatisticsRecorder::FindHistogram(name, &h)) {
    FlagHistogram *fh = new FlagHistogram(name);
    fh->InitializeBucketRange();
    fh->SetFlags(flags);
    size_t zero_index = fh->BucketIndex(0);
    fh->LinearHistogram::Accumulate(0, 1, zero_index);
    h = StatisticsRecorder::RegisterOrDeleteDuplicate(fh);
  }

  return h;
}

FlagHistogram::FlagHistogram(const std::string &name)
  : BooleanHistogram(name), mSwitched(false) {
}

Histogram::ClassType
FlagHistogram::histogram_type() const
{
  return FLAG_HISTOGRAM;
}

void
FlagHistogram::Accumulate(Sample value, Count count, size_t index)
{
  if (mSwitched) {
    return;
  }

  mSwitched = true;
  DCHECK_EQ(value, 1);
  LinearHistogram::Accumulate(value, 1, index);
  size_t zero_index = BucketIndex(0);
  LinearHistogram::Accumulate(0, -1, zero_index);
}

void
FlagHistogram::AddSampleSet(const SampleSet& sample) {
  DCHECK_EQ(bucket_count(), sample.size());
  // We can't be sure the SampleSet provided came from another FlagHistogram,
  // so we take the following steps:
  //  - If our flag has already been set do nothing.
  //  - Set our flag if the following hold:
  //      - The sum of the counts in the provided SampleSet is 1.
  //      - The bucket index for that single value is the same as the index where we
  //        would place our set flag.
  //  - Otherwise, take no action.

  if (mSwitched) {
    return;
  }

  if (sample.sum() != 1) {
    return;
  }

  size_t one_index = BucketIndex(1);
  if (sample.counts(one_index) == 1) {
    Accumulate(1, 1, one_index);
  }
}

void
FlagHistogram::Clear() {
  Histogram::Clear();

  mSwitched = false;
  size_t zero_index = BucketIndex(0);
  LinearHistogram::Accumulate(0, 1, zero_index);
}

//------------------------------------------------------------------------------
// CountHistogram:
//------------------------------------------------------------------------------

Histogram *
CountHistogram::FactoryGet(const std::string &name, Flags flags)
{
  Histogram *h(nullptr);

  if (!StatisticsRecorder::FindHistogram(name, &h)) {
    CountHistogram *fh = new CountHistogram(name);
    fh->InitializeBucketRange();
    fh->SetFlags(flags);
    h = StatisticsRecorder::RegisterOrDeleteDuplicate(fh);
  }

  return h;
}

CountHistogram::CountHistogram(const std::string &name)
  : LinearHistogram(name, 1, 2, 3) {
}

Histogram::ClassType
CountHistogram::histogram_type() const
{
  return COUNT_HISTOGRAM;
}

void
CountHistogram::Accumulate(Sample value, Count count, size_t index)
{
  size_t zero_index = BucketIndex(0);
  LinearHistogram::Accumulate(value, 1, zero_index);
}

void
CountHistogram::AddSampleSet(const SampleSet& sample) {
  DCHECK_EQ(bucket_count(), sample.size());
  // We can't be sure the SampleSet provided came from another CountHistogram,
  // so we at least check that the unused buckets are empty.

  const size_t indices[] = { BucketIndex(0), BucketIndex(1), BucketIndex(2) };

  if (sample.counts(indices[1]) != 0 || sample.counts(indices[2]) != 0) {
    return;
  }

  if (sample.counts(indices[0]) != 0) {
    Accumulate(1, sample.counts(indices[0]), indices[0]);
  }
}


//------------------------------------------------------------------------------
// CustomHistogram:
//------------------------------------------------------------------------------

Histogram* CustomHistogram::FactoryGet(const std::string& name,
                                       const std::vector<Sample>& custom_ranges,
                                       Flags flags) {
  Histogram* histogram(NULL);

  // Remove the duplicates in the custom ranges array.
  std::vector<int> ranges = custom_ranges;
  ranges.push_back(0);  // Ensure we have a zero value.
  std::sort(ranges.begin(), ranges.end());
  ranges.erase(std::unique(ranges.begin(), ranges.end()), ranges.end());
  if (ranges.size() <= 1) {
    DCHECK(false);
    // Note that we pushed a 0 in above, so for defensive code....
    ranges.push_back(1);  // Put in some data so we can index to [1].
  }

  DCHECK_LT(ranges.back(), kSampleType_MAX);

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    CustomHistogram* tentative_histogram = new CustomHistogram(name, ranges);
    tentative_histogram->InitializedCustomBucketRange(ranges);
    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }

  DCHECK_EQ(histogram->histogram_type(), CUSTOM_HISTOGRAM);
  DCHECK(histogram->HasConstructorArguments(ranges[1], ranges.back(),
                                            ranges.size()));
  return histogram;
}

Histogram::ClassType CustomHistogram::histogram_type() const {
  return CUSTOM_HISTOGRAM;
}

CustomHistogram::CustomHistogram(const std::string& name,
                                 const std::vector<Sample>& custom_ranges)
    : Histogram(name, custom_ranges[1], custom_ranges.back(),
                custom_ranges.size()) {
  DCHECK_GT(custom_ranges.size(), 1u);
  DCHECK_EQ(custom_ranges[0], 0);
}

void CustomHistogram::InitializedCustomBucketRange(
    const std::vector<Sample>& custom_ranges) {
  DCHECK_GT(custom_ranges.size(), 1u);
  DCHECK_EQ(custom_ranges[0], 0);
  DCHECK_LE(custom_ranges.size(), bucket_count());
  for (size_t index = 0; index < custom_ranges.size(); ++index)
    SetBucketRange(index, custom_ranges[index]);
  ResetRangeChecksum();
}

double CustomHistogram::GetBucketSize(Count current, size_t i) const {
  return 1;
}

//------------------------------------------------------------------------------
// The next section handles global (central) support for all histograms, as well
// as startup/teardown of this service.
//------------------------------------------------------------------------------

// This singleton instance should be started during the single threaded portion
// of main(), and hence it is not thread safe.  It initializes globals to
// provide support for all future calls.
StatisticsRecorder::StatisticsRecorder() {
  DCHECK(!histograms_);
  if (lock_ == NULL) {
    // This will leak on purpose. It's the only way to make sure we won't race
    // against the static uninitialization of the module while one of our
    // static methods relying on the lock get called at an inappropriate time
    // during the termination phase. Since it's a static data member, we will
    // leak one per process, which would be similar to the instance allocated
    // during static initialization and released only on  process termination.
    lock_ = new base::Lock;
  }
  base::AutoLock auto_lock(*lock_);
  histograms_ = new HistogramMap;
}

StatisticsRecorder::~StatisticsRecorder() {
  DCHECK(histograms_ && lock_);

  if (dump_on_exit_) {
    std::string output;
    WriteGraph("", &output);
    CHROMIUM_LOG(INFO) << output;
  }
  // Clean up.
  HistogramMap* histograms = NULL;
  {
    base::AutoLock auto_lock(*lock_);
    histograms = histograms_;
    histograms_ = NULL;
    for (HistogramMap::iterator it = histograms->begin();
         histograms->end() != it;
         ++it) {
      // No other clients permanently hold Histogram references, so we
      // have the only one and it is safe to delete it.
      delete it->second;
    }
  }
  delete histograms;
  // We don't delete lock_ on purpose to avoid having to properly protect
  // against it going away after we checked for NULL in the static methods.
}

// static
bool StatisticsRecorder::IsActive() {
  if (lock_ == NULL)
    return false;
  base::AutoLock auto_lock(*lock_);
  return NULL != histograms_;
}

Histogram* StatisticsRecorder::RegisterOrDeleteDuplicate(Histogram* histogram) {
  DCHECK(histogram->HasValidRangeChecksum());
  if (lock_ == NULL)
    return histogram;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return histogram;
  const std::string name = histogram->histogram_name();
  HistogramMap::iterator it = histograms_->find(name);
  // Avoid overwriting a previous registration.
  if (histograms_->end() == it) {
    (*histograms_)[name] = histogram;
  } else {
    delete histogram;  // We already have one by this name.
    histogram = it->second;
  }
  return histogram;
}

// static
void StatisticsRecorder::WriteHTMLGraph(const std::string& query,
                                        std::string* output) {
  if (!IsActive())
    return;
  output->append("<html><head><title>About Histograms");
  if (!query.empty())
    output->append(" - " + query);
  output->append("</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteHTMLGraph(output);
    output->append("<br><hr><br>");
  }
  output->append("</body></html>");
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (!IsActive())
    return;
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteAscii(true, "\n", output);
    output->append("\n");
  }
}

// static
void StatisticsRecorder::GetHistograms(Histograms* output) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    DCHECK_EQ(it->first, it->second->histogram_name());
    output->push_back(it->second);
  }
}

bool StatisticsRecorder::FindHistogram(const std::string& name,
                                       Histogram** histogram) {
  if (lock_ == NULL)
    return false;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return false;
  HistogramMap::iterator it = histograms_->find(name);
  if (histograms_->end() == it)
    return false;
  *histogram = it->second;
  return true;
}

// private static
void StatisticsRecorder::GetSnapshot(const std::string& query,
                                     Histograms* snapshot) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    if (it->first.find(query) != std::string::npos)
      snapshot->push_back(it->second);
  }
}

// static
StatisticsRecorder::HistogramMap* StatisticsRecorder::histograms_ = NULL;
// static
base::Lock* StatisticsRecorder::lock_ = NULL;
// static
bool StatisticsRecorder::dump_on_exit_ = false;

}  // namespace base
