CXXFLAGS = -O3 -Wall
CXX = g++


hailstone: hailstone.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<


clean:
	rm -f hailstone *.o

