This is my entry for the Intel Threading Challenge, Phase 2, Problem 2,
Amateur Level.

Pre-requisites
==============

- Threading Building Blocks (TBB). Tested with 3.0, may work(?) with earlier
  versions.
- g++. Tested with 4.2.1 on OS X and 4.4.1 on Linux. Should work with other
  versions too.

To build it
===========

- Make sure the environment variables for 64-bit TBB are set up (e.g. by
  running the tbbvars.sh script that ships with TBB).
- Run make

That should be it. The executable it generates is called 'hailstone'. It
compiles cleanly on the OS X (Snow Leopard, 64-bit) and Linux (Ubuntu 9.10,
64-bit) machines that I've tested.


To run it
=========

  ./hailstone <lower> <upper> <max-length> <bucket-size>

(as in the problem description).

The output includes the time taken to calculate all the sequence lengths for
the range (excluding printing them out at the end), using TBB's tick_count
class.
