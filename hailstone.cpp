#include <cstdlib>
#include <cstdio>
#include <cstring>


static const size_t kTrailingBitsMask = 64 - 1;
static const unsigned char kNumTrailingZeroBits[64] = {
  6, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0
};


static const size_t kNumStoredSequences = (1 << 20);
static size_t gSequenceLength[kNumStoredSequences];


inline size_t HailstoneSequenceLength(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = 0;
  while (length <= maxLength) {
    if (val < kNumStoredSequences && gSequenceLength[val] != 0) {
      length += gSequenceLength[val];
      break;
    }
    size_t numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    while (numTrailingZeros > 0) {
      val >>= numTrailingZeros;
      length += numTrailingZeros;
      numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    }
    if (val == 1 || length > maxLength) {
      length += 1;
      break;
    }
    val = 3 * val + 1;
    ++length;
  }
  if (start < kNumStoredSequences && gSequenceLength[start] == 0)
    gSequenceLength[start] = length;

  return length;
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

  memset(gSequenceLength, 0, sizeof(size_t) * kNumStoredSequences);
  gSequenceLength[1] = 1;

  // The important bit!
  for (size_t start = lower; start <= upper; ++start) {
    size_t len = HailstoneSequenceLength(start, maxLength);
    if (len > maxLength)
      ++overflow;
    else
      ++buckets[(len - 1) / bucketSize];
  }

  printf("Counts of hailstone sequence lengths for range %ld-%ld:\n", lower, upper);
  for (size_t i = 0; i < numBuckets; ++i) {
    size_t low = i * bucketSize + 1;
    size_t high = (i + 1) * bucketSize;
    if (high > maxLength)
      high = maxLength;
    printf("%ld-%ld:\t%ld\n", low, high, buckets[i]);
  }
  printf("%ld+:\t%ld\n", maxLength + 1, overflow);

  return 0;
}

