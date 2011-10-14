libgit2=/home/nelhage/sw/libgit2
CXXFLAGS=-I$(libgit2)/include
LDFLAGS=-L$(libgit2)/lib -lgit2

all: wc
