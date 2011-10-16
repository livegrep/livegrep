-include Makefile.config

ifneq ($(libgit2),)
	CXXFLAGS += -I$(libgit2)/include
	LDFLAGS += -L$(libgit2)/lib -Wl,-R$(libgit2)/lib
endif
ifneq ($(re2),)
	CXXFLAGS += -I$(re2)/include
	LDFLAGS += -L$(re2)/lib -Wl,-R$(re2)/lib
endif

CXXFLAGS +=-ggdb3 -std=c++0x -Wall -Werror -O2
LDFLAGS += $(LIBS)
LIBS=-lgit2 -lre2

HEADERS = smart_git.h timer.h

all: codesearch

codesearch.o: codesearch.cc $(HEADERS)

clean:
	rm -f codesearch codesearch.o
