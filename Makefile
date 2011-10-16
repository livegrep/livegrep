libgit2=/home/nelhage/sw/libgit2
CXXFLAGS=-I$(libgit2)/include -ggdb3 -std=c++0x
LDFLAGS=-L$(libgit2)/lib -lgit2 -Wl,-R$(libgit2)/lib

all: wc
