CXXFLAGS = -O3 -Wall -fopenmp
CXX = g++


hailstone: hailstone.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<


clean:
	rm -f hailstone *.o

