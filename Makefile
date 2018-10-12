CXX=g++
CXXFLAGS=-Wall -Werror -std=c++11 -O3

.PHONY:
all: fastcopy

.PHONY:
clean:
	-rm fastcopy *.o

fastcopy: main.o
	$(CXX) $(CXXFLAGS) -o fastcopy $^

.PHONY:
reformat:
	clang-format -i *.cc

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

