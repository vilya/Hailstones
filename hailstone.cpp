#include <cstdlib>
#include <cstdio>

#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <tbb/tick_count.h>


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


inline size_t HailstoneSequenceLength(size_t start, size_t maxLength)
{
  size_t val = start;
  size_t length = 1;
  //printf("[%lu]", val);
  while (length <= maxLength) {
    size_t numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    while (numTrailingZeros > 0) {
      val >>= numTrailingZeros;
      length += numTrailingZeros;
      //printf(" -> %lu", val);
      numTrailingZeros = kNumTrailingZeroBits[val & kTrailingBitsMask];
    }
    if (val == 1 || length > maxLength)
      break;
    val = 3 * val + 1;
    ++length;
    //printf(" -> %lu", val);
  }
  //printf("\t[length: %lu]\n", length);
  return length;
}


struct HailstoneGatherer
{
  const size_t _numBuckets;
  const size_t _maxLength;
  const size_t _bucketSize;

  size_t* _buckets;
  size_t _overflow;


  HailstoneGatherer(size_t numBuckets, size_t maxLength, size_t bucketSize) :
    _numBuckets(numBuckets),
    _maxLength(maxLength),
    _bucketSize(bucketSize),
    _buckets(new size_t[numBuckets]),
    _overflow(0)
  {
    for (size_t i = 0; i < _numBuckets; ++i)
      _buckets[i] = 0;
  }


  HailstoneGatherer(HailstoneGatherer& other, tbb::split) :
    _numBuckets(other._numBuckets),
    _maxLength(other._maxLength),
    _bucketSize(other._bucketSize),
    _buckets(new size_t[other._numBuckets]),
    _overflow(0)
  {
    for (size_t i = 0; i < _numBuckets; ++i)
      _buckets[i] = 0;
  }


  ~HailstoneGatherer()
  {
    delete[] _buckets;
  }


  void operator () (const tbb::blocked_range<size_t>& range)
  {
    const size_t maxLength = _maxLength;
    const size_t bucketSize = _bucketSize;

    size_t* buckets = _buckets;
    size_t overflow = _overflow;

    for (size_t i = range.begin(); i != range.end(); ++i) {
      size_t len = HailstoneSequenceLength(i, maxLength);
      if (len > maxLength)
        ++overflow;
      else
        ++buckets[(len - 1) / bucketSize];
    }

    _overflow = overflow;
  }


  void join(HailstoneGatherer& rhs)
  {
    for (size_t i = 0; i < _numBuckets; ++i)
      _buckets[i] += rhs._buckets[i];
    _overflow += rhs._overflow;
  }
};


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

  tbb::tick_count startTime = tbb::tick_count::now();

  size_t numBuckets = maxLength / bucketSize;
  if (maxLength % bucketSize != 0)
    ++numBuckets;

  HailstoneGatherer gatherer(numBuckets, maxLength, bucketSize);
  tbb::parallel_reduce(tbb::blocked_range<size_t>(lower, upper), gatherer);

  tbb::tick_count endTime = tbb::tick_count::now();
  printf("Counting finished in %g seconds.\n", (endTime - startTime).seconds());

  size_t* buckets = gatherer._buckets;
  size_t overflow = gatherer._overflow;
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

