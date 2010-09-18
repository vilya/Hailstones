#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <tbb/tick_count.h>


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

  size_t numBuckets = maxLength / bucketSize;
  if (maxLength % bucketSize != 0)
    ++numBuckets;

  size_t* buckets = new size_t[numBuckets];
  for (size_t i = 0; i < numBuckets; ++i)
    buckets[i] = 0;
  size_t overflow = 0;

  // Start timing.
  tbb::tick_count startTime = tbb::tick_count::now();
  
  // Fill in the lookup table for the number of trailing zero bits.
  PopulateTrailingZeroBits();

  // Fill in the lookup table for sequence lengths of numbers up to kNumStoredSequences.
  gSequenceLength[0] = 0;
  gSequenceLength[1] = 1;
  for (size_t i = 2; i < kNumStoredSequences; ++i)
    gSequenceLength[i] = HailstoneSequenceLengthUnstored(i, maxLength);
  
  // Calculate the sequence lengths for the input range, using the lookup tables
  // where possible.
  for (size_t start = lower; start <= upper; ++start) {
    size_t len = HailstoneSequenceLengthStored(start, maxLength);
    if (len > maxLength)
      ++overflow;
    else
      ++buckets[(len - 1) / bucketSize];
  }

  // Stop timing.
  tbb::tick_count endTime = tbb::tick_count::now();

  // Print the results.
  PrintResults(startTime, endTime,
      lower, upper, maxLength, bucketSize,
      numBuckets, buckets, overflow);

  return 0;
}

