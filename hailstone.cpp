#include <algorithm>
#include <cstdlib>
#include <cstdio>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/tick_count.h>


//
// Type declarations
//

// Used to pre-fill the sequence length table.
struct HailstoneFiller
{
  const size_t _maxLength;

  HailstoneFiller(size_t maxLength);
  void operator () (const tbb::blocked_range<size_t>& range) const;
};


// Use to calculate sequence lengths using the lookup table where possible, but
// falling back to calculating the hard way where necessary.
struct HailstoneGathererFull
{
  const size_t _numBuckets;
  const size_t _maxLength;
  const size_t _bucketSize;
  const size_t _lower;
  const size_t _upper;

  size_t* _buckets;
  size_t _overflow;

  HailstoneGathererFull(size_t numBuckets, size_t maxLength, size_t bucketSize, size_t lower, size_t upper);
  HailstoneGathererFull(HailstoneGathererFull& other, tbb::split);
  ~HailstoneGathererFull();

  void operator () (const tbb::blocked_range<size_t>& range);
  void join(HailstoneGathererFull& rhs);
};


//
// Function declarations
//

void PopulateTrailingZeroBits();
inline size_t HailstoneSequenceLengthUnstored(size_t start, size_t maxLength);
inline size_t HailstoneSequenceLengthStored(size_t start, size_t maxLength);


//
// Constants
//

static const size_t kTrailingBitsMaskSize = 8;        // Tunable parameter.
static const size_t kNumStoredSequences = (1 << 20);  // Tunable parameter.

static const size_t kTrailingBitsLimit = 1 << kTrailingBitsMaskSize;
static const size_t kTrailingBitsMask = kTrailingBitsLimit - 1;

// Lookup table for the number of trailing zero bits in an 8 bit number.
// This is written to only during the PopulateTrailingZeroBits function.
// Anywhere else, it should be treated as a constant.
static size_t kNumTrailingZeroBits[kTrailingBitsLimit];



//
// Globals
//

static size_t gSequenceLength[kNumStoredSequences];


//
// HailstoneFiller methods
//

HailstoneFiller::HailstoneFiller(size_t maxLength) :
  _maxLength(maxLength)
{
}


void HailstoneFiller::operator () (const tbb::blocked_range<size_t>& range) const
{
  const size_t maxLength = _maxLength;

  size_t i = range.begin();
  i += 1 - (i & 0x1); // Make sure i is odd by incrementing it if it's even.
  for (; i < range.end(); i += 2) {
    size_t len = HailstoneSequenceLengthUnstored(i, maxLength);
    size_t val = i;
    while (val < kNumStoredSequences) {
      gSequenceLength[val] = len;
      val <<= 1;
      ++len;
    }
  }
}


//
// HailstoneGathererFull methods
//

HailstoneGathererFull::HailstoneGathererFull(size_t numBuckets, size_t maxLength, size_t bucketSize,
    size_t lower, size_t upper) :
  _numBuckets(numBuckets),
  _maxLength(maxLength),
  _bucketSize(bucketSize),
  _lower(lower),
  _upper(upper),
  _buckets(new size_t[numBuckets]),
  _overflow(0)
{
  for (size_t i = 0; i < _numBuckets; ++i)
    _buckets[i] = 0;
}


HailstoneGathererFull::HailstoneGathererFull(HailstoneGathererFull& other, tbb::split) :
  _numBuckets(other._numBuckets),
  _maxLength(other._maxLength),
  _bucketSize(other._bucketSize),
  _lower(other._lower),
  _upper(other._upper),
  _buckets(new size_t[other._numBuckets]),
  _overflow(0)
{
  for (size_t i = 0; i < _numBuckets; ++i)
    _buckets[i] = 0;
}


HailstoneGathererFull::~HailstoneGathererFull()
{
  delete[] _buckets;
}


void HailstoneGathererFull::operator () (const tbb::blocked_range<size_t>& range)
{
  const size_t maxLength = _maxLength;
  const size_t bucketSize = _bucketSize;
  const size_t upper = _upper;

  size_t* buckets = _buckets;
  size_t overflow = _overflow;

  const size_t kSplit = std::min(_lower * 2, upper);
  if (kSplit >= upper) {
    for (size_t i = range.begin(); i < range.end(); ++i) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      if (len <= maxLength)
        ++buckets[(len - 1) / bucketSize];
      else
        ++overflow;
    }
  }
  else {
    size_t i;
    for (i = range.begin(); i < kSplit; ++i) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      size_t val = i;
      while (val <= upper && len <= maxLength) {
        ++buckets[(len - 1) / bucketSize];
        val <<= 1;
        ++len;
      }
      while (val <= upper) {
        ++overflow;
        val <<= 1;
      }
    }
    if ((i & 0x1) == 0)
      ++i;
    for (; i < range.end(); i += 2) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      size_t val = i;
      while (val <= upper && len <= maxLength) {
        ++buckets[(len - 1) / bucketSize];
        val <<= 1;
        ++len;
      }
      while (val <= upper) {
        ++overflow;
        val <<= 1;
      }
    }
  }
  _overflow = overflow;
}


void HailstoneGathererFull::join(HailstoneGathererFull& rhs)
{
  for (size_t i = 0; i < _numBuckets; ++i)
    _buckets[i] += rhs._buckets[i];
  _overflow += rhs._overflow;
}


//
// Functions
//

void PopulateTrailingZeroBits()
{
  kNumTrailingZeroBits[0] = kTrailingBitsMaskSize;
  for (size_t i = 1; i < kTrailingBitsLimit; ++i) {
    size_t val = i;
    kNumTrailingZeroBits[i] = 0;
    while ((val & 0x1) == 0) {
      val >>= 1;
      ++kNumTrailingZeroBits[i];
    }
  }
}


inline size_t HailstoneSequenceLengthUnstored(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = 1;
  while (length <= maxLength && val != 1) {
    if ((val & 0x1) != 0) {
      val = (val << 1) + val + 1;
      ++length;
    }
    size_t numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    val >>= numTrailingZeros;
    length += numTrailingZeros;
  }

  return length;
}


inline size_t HailstoneSequenceLengthStored(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = 0;
  while (length <= maxLength && val >= kNumStoredSequences) {
    if ((val & 0x1) != 0) {
      val = (val << 1) + val + 1;
      ++length;
    }
    size_t numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    val >>= numTrailingZeros;
    length += numTrailingZeros;
  }

  if (length <= maxLength)
    length += gSequenceLength[val];

  return length;
}


void PrintResults(const tbb::tick_count& startTime, const tbb::tick_count& endTime,
    size_t lower, size_t upper, size_t maxLength, size_t bucketSize,
    size_t numBuckets, size_t* buckets, size_t overflow)
{
  size_t total = overflow;

  printf("Counts of hailstone sequence lengths for range %ld-%ld:\n", lower, upper);
  for (size_t i = 0; i < numBuckets; ++i) {
    size_t low = i * bucketSize + 1;
    size_t high = (i + 1) * bucketSize;
    if (high > maxLength)
      high = maxLength;
    printf("%ld-%ld:\t%ld\n", low, high, buckets[i]);
    total += buckets[i];
  }
  printf("%ld+:\t%ld\n", maxLength + 1, overflow);
  printf("Total:\t%ld\n", total);
  printf("Counting finished in %g seconds.\n", (endTime - startTime).seconds());
}


int main(int argc, char** argv)
{
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <lower> <upper> <max-length> <bucket-size>\n", argv[0]);
    return -1;
  }

  size_t lower = atoll(argv[1]);
  size_t upper = atoll(argv[2]);
  size_t maxLength = atoll(argv[3]);
  size_t bucketSize = atoll(argv[4]);

  // Start timing.
  tbb::tick_count startTime = tbb::tick_count::now();
  
  // Fill in the lookup table for the number of trailing zero bits.
  PopulateTrailingZeroBits();
  size_t numBuckets = maxLength / bucketSize;
  if (maxLength % bucketSize != 0)
    ++numBuckets;

  // Fill in the lookup table for sequence lengths of numbers up to kNumStoredSequences.
  gSequenceLength[0] = 0;
  HailstoneFiller filler(maxLength);
  tbb::parallel_for(tbb::blocked_range<size_t>(1, kNumStoredSequences), filler);

  // Calculate the sequence lengths for the input range, using the lookup tables
  // where possible.
  size_t* buckets;
  size_t overflow;

  HailstoneGathererFull gatherFull(numBuckets, maxLength, bucketSize, lower, upper);
  tbb::parallel_reduce(tbb::blocked_range<size_t>(lower, upper + 1), gatherFull);
  buckets = gatherFull._buckets;
  overflow = gatherFull._overflow;

  // Stop timing.
  tbb::tick_count endTime = tbb::tick_count::now();

  // Print the results.
  PrintResults(startTime, endTime,
      lower, upper, maxLength, bucketSize, numBuckets, buckets, overflow);

  return 0;
}

