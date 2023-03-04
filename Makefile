CXX=clang++ -std=c++20
CPPFLAGS=-O2 -Wall -Wextra -Wno-sign-compare -Wshadow
LDFLAGS=
SRCS=$(shell find src -type f -name '*.cpp')
OBJS=$(subst .cpp,.o,$(SRCS))

all: build/mc_maxsat

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $<

build/mc_maxsat: $(OBJS)
	@mkdir -p build
	$(CXX) $(LDFLAGS) -o build/mc_maxsat $(OBJS)

clean:
	rm $(OBJS) build/*
