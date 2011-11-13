-include Makefile.config

comma:=,

libre2=$(CURDIR)/re2/obj/libre2.a

extradirs=$(sort $(libgit2) $(gflags))

CPPFLAGS = -I$(CURDIR)/re2/ $(patsubst %,-I%/include, $(extradirs))
LDFLAGS  = $(patsubst %, -L%/lib, $(extradirs))
LDFLAGS += $(patsubst %, -Wl$(comma)-R%/lib, $(extradirs))

CXXFLAGS+=-ggdb3 -std=c++0x -Wall -Werror -Wno-sign-compare -pthread
LDFLAGS+=-pthread
LDLIBS=-lgit2 -ljson -lgflags

ifeq ($(noopt),)
CXXFLAGS+=-O2
endif
ifneq ($(densehash),)
CXXFLAGS+=-DUSE_DENSE_HASH_SET
endif
ifneq ($(profile),)
CXXFLAGS+=-DPROFILE_CODESEARCH
endif

HEADERS = smart_git.h timer.h thread_queue.h mutex.h thread_pool.h codesearch.h chunk.h chunk_allocator.h
OBJECTS = codesearch.o main.o chunk.o chunk_allocator.o radix_sort.o
DEPFILES = $(OBJECTS:%.o=.%.d)

all: codesearch $(DEPFILES)

codesearch: $(OBJECTS) $(libre2)

$(libre2):
	( cd re2 && $(MAKE) )

clean:
	rm -f codesearch $(OBJECTS) $(DEPFILES)

.%.d: %.cc
	@set -e; rm -f $@; \
	 $(CXX) -M $(CPPFLAGS) $(CXXFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

-include $(DEPFILES)
