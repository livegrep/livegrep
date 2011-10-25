-include Makefile.config

comma:=,

extradirs=$(sort $(libgit2) $(re2))

CXXFLAGS = $(patsubst %,-I%/include, $(extradirs))
LDFLAGS  = $(patsubst %, -L%/lib, $(extradirs))
LDFLAGS += $(patsubst %, -Wl$(comma)-R%/lib, $(extradirs))

CXXFLAGS+=-ggdb3 -std=c++0x -Wall -Werror -Wno-sign-compare -pthread
LDFLAGS+=-pthread
LDLIBS=-lgit2 -lre2

ifeq ($(noopt),)
CXXFLAGS+=-O2
endif
ifneq ($(profile),)
CXXFLAGS+=-pg
LDFLAGS+=-pg
endif

HEADERS = smart_git.h timer.h thread_queue.h mutex.h thread_pool.h codesearch.h

all: codesearch

codesearch: codesearch.o main.o

codesearch.o: codesearch.cc $(HEADERS)

clean:
	rm -f codesearch codesearch.o
