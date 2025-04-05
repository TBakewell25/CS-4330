
CXX = g++
CXXFLAGS= -g -Wall -std=c++11 #-DENABLE_DEBUG
OPTFLAGS= -O3

EXE_NAME=processor
SRCS := main.cpp memory.cpp processor.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(EXE_NAME)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

processor.o: regfile.h ALU.h control.h processor.h
memory.o: memory.h
main.o: memory.h processor.h

clean:
	$(RM) $(EXE_NAME) $(OBJS)


