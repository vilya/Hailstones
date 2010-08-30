def hailstone(n):
  '''Compute the next "Hailstone Number" in the sequence after n.'''
  if n % 2 == 0:
    return n / 2
  else:
    return 3 * n + 1


def hailstone_len(start, maxlength):
  val = start
  count = 1
  while val != 1 and count <= maxlength:
    val = hailstone(val)
    count += 1
  return count


if __name__ == '__main__':
  import sys
  lower, upper, maxlength, bucketsize = [int(arg) for arg in sys.argv[1:]]
  num_buckets = (maxlength / bucketsize)
  if maxlength % bucketsize != 0:
    num_buckets += 1

  buckets = [0] * num_buckets
  overflow = 0
  for start in xrange(lower, upper + 1):
    l = hailstone_len(start, maxlength)
    if l > maxlength:
      overflow += 1
    else:
      buckets[(l - 1) / bucketsize] += 1

  print "Counts of hailstone sequence lengths for range %d-%d:" % (lower, upper)
  for i in xrange(num_buckets):
    low = i * bucketsize + 1
    high = min((i + 1) * bucketsize, maxlength)
    print "%d-%d:\t%d" % (low, high, buckets[i])

  print "%d+:\t%d" % (maxlength + 1, overflow)
