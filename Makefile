CXX=g++
CXXFLAGS=-Wall -Werror -std=c++11 -O3

.PHONY:
all: splicecopy

.PHONY:
clean:
	-rm -f splicecopy *.o

splicecopy: main.o
	$(CXX) $(CXXFLAGS) -o splicecopy $^

.PHONY:
reformat:
	clang-format -style=Google -i *.cc

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

