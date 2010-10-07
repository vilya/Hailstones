#include <algorithm>
#include <cstdlib>
#include <cstdio>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/tick_count.h>


//
// Constants
//

static const size_t kTrailingBitsMaskSize = 8;        // Tunable parameter.
static const size_t kNumStoredSequences = (1 << 20);  // Tunable parameter.

// Maximum possible sequence length for numbers less than 2^32 is 1137. We
// leave a tiny bit of wiggle room though...
static const size_t kMaxPossibleLength = 1140;


//
// Globals
//

static size_t gSequenceLength[kNumStoredSequences];


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
  const size_t _maxLength;
  const size_t _lower;
  const size_t _upper;

  size_t _buckets[kMaxPossibleLength];

  HailstoneGathererFull(size_t maxLength, size_t lower, size_t upper);
  HailstoneGathererFull(HailstoneGathererFull& other, tbb::split);

  void operator () (const tbb::blocked_range<size_t>& range);
  void join(HailstoneGathererFull& rhs);
};


//
// Function declarations
//

inline size_t HailstoneSequenceLengthUnstored(size_t start, size_t maxLength);
inline size_t HailstoneSequenceLengthStored(size_t start, size_t maxLength);
inline void FillBuckets(const size_t lengthCounts[], size_t maxLength,
    size_t bucketSize, size_t numBuckets, size_t buckets[], size_t& overflow);
void PrintResults(const tbb::tick_count& startTime, const tbb::tick_count& endTime,
    size_t lower, size_t upper, size_t maxLength, size_t bucketSize,
    size_t numBuckets, size_t* buckets, size_t overflow);


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

HailstoneGathererFull::HailstoneGathererFull(size_t maxLength, size_t lower, size_t upper) :
  _maxLength(maxLength),
  _lower(lower),
  _upper(upper)
{
  memset(_buckets, 0, sizeof(size_t) * kMaxPossibleLength);
}


HailstoneGathererFull::HailstoneGathererFull(HailstoneGathererFull& other, tbb::split) :
  _maxLength(other._maxLength),
  _lower(other._lower),
  _upper(other._upper)
{
  memset(_buckets, 0, sizeof(size_t) * kMaxPossibleLength);
}


void HailstoneGathererFull::operator () (const tbb::blocked_range<size_t>& range)
{
  const size_t maxLength = _maxLength;
  const size_t upper = _upper;

  size_t* buckets = _buckets;

  const size_t kSplit = std::min(_lower * 2, range.end());
  if (kSplit >= upper) {
    for (size_t i = range.begin(); i < range.end(); ++i) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      ++buckets[len];
    }
  }
  else {
    // First pass: even numbers.
    size_t i = range.begin();
    i += (i & 0x1);
    for (; i < kSplit; i += 2) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      size_t val = i;
      while (val <= upper) {
        ++buckets[len];
        val <<= 1;
        ++len;
      }
    }

    // Second pass: odd numbers.
    i = range.begin();
    i += 1 - (i & 0x1);
    for (; i < range.end(); i += 2) {
      size_t len = HailstoneSequenceLengthStored(i, maxLength);
      size_t val = i;
      while (val <= upper) {
        ++buckets[len];
        val <<= 1;
        ++len;
      }
    }
  }
}


void HailstoneGathererFull::join(HailstoneGathererFull& rhs)
{
  for (size_t i = 0; i < kMaxPossibleLength; ++i)
    _buckets[i] += rhs._buckets[i];
}


//
// Functions
//

inline size_t HailstoneSequenceLengthUnstored(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = __builtin_ctzll(val);
  val >>= length;
  
  while (length <= maxLength && val != 1) {
    val = (val << 1) + val + 1;
    ++length;
 
    size_t numTrailingZeros = __builtin_ctzll(val);
    val >>= numTrailingZeros;
    length += numTrailingZeros;
  }

  return length + 1;
}


inline size_t HailstoneSequenceLengthStored(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = __builtin_ctzll(val);
  val >>= length;

  while (length <= maxLength && val >= kNumStoredSequences) {
    val = (val << 1) + val + 1;
    ++length;

    size_t numTrailingZeros = __builtin_ctzll(val);
    val >>= numTrailingZeros;
    length += numTrailingZeros;
  }

  if (length <= maxLength)
    length += gSequenceLength[val];

  return length;
}


inline void FillBuckets(const size_t lengthCounts[], size_t maxLength, size_t bucketSize, size_t numBuckets, size_t buckets[], size_t& overflow)
{
  for (size_t i = 0; i < numBuckets; ++i) {
    size_t low = i * bucketSize + 1;
    size_t high = (i + 1) * bucketSize;
    if (high > kMaxPossibleLength - 1)
      high = kMaxPossibleLength - 1;
    else if (high > maxLength)
      high = maxLength;

    buckets[i] = 0;
    for (size_t j = low; j <= high; ++j)
      buckets[i] += lengthCounts[j];
  }

  overflow = 0;
  for (size_t j = maxLength + 1; j < kMaxPossibleLength; ++j)
    overflow += lengthCounts[j];
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
  
  // Fill in the lookup table for sequence lengths of numbers up to kNumStoredSequences.
  gSequenceLength[0] = 0;
  HailstoneFiller filler(maxLength);
  tbb::parallel_for(tbb::blocked_range<size_t>(1, kNumStoredSequences), filler);

  // Calculate the sequence lengths for the input range, using the lookup tables
  // where possible.
  HailstoneGathererFull gatherFull(maxLength, lower, upper);
  tbb::parallel_reduce(tbb::blocked_range<size_t>(lower, upper + 1), gatherFull);

  // Combine the length counts into their buckets.
  size_t numBuckets = maxLength / bucketSize;
  if (maxLength % bucketSize != 0)
    ++numBuckets;
  size_t buckets[kMaxPossibleLength];
  size_t overflow;
  FillBuckets(gatherFull._buckets, maxLength, bucketSize, numBuckets, buckets, overflow);

  // Stop timing.
  tbb::tick_count endTime = tbb::tick_count::now();

  // Print the results.
  PrintResults(startTime, endTime,
      lower, upper, maxLength, bucketSize, numBuckets, buckets, overflow);

  return 0;
}

