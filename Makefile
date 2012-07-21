include Makefile.lib
-include Makefile.config

libre2=$(CURDIR)/re2/obj/libre2.a

extradirs=$(sort $(libgit2) $(gflags))

override CPPFLAGS += -I$(CURDIR)/re2/ $(patsubst %,-I%/include, $(extradirs))
override LDFLAGS += $(patsubst %, -L%/lib, $(extradirs))
override LDFLAGS += $(patsubst %, -Wl$(comma)-R%/lib, $(extradirs))

override CXXFLAGS+=-g -std=c++0x -Wall -Werror -Wno-sign-compare -pthread
override LDFLAGS+=-pthread
LDLIBS=-lgit2 -ljson -lgflags

ifeq ($(noopt),)
override CXXFLAGS+=-O2
endif
ifneq ($(densehash),)
override CXXFLAGS+=-DUSE_DENSE_HASH_SET
endif
ifneq ($(slowgtod),)
override CXXFLAGS+=-DCODESEARCH_SLOWGTOD
endif
ifneq ($(tcmalloc),)
override LDLIBS+=-ltcmalloc
endif

OBJECTS = codesearch.o main.o chunk.o \
          chunk_allocator.o radix_sort.o \
          dump_load.o indexer.o re_width.o \
          debug.o
DEPFILES = $(OBJECTS:%.o=.%.d)

all: codesearch $(DEPFILES)

codesearch: $(OBJECTS) $(libre2) .config/LDFLAGS
	$(CXX) -o $@ $(LDFLAGS) $(filter-out .config/%,$^) $(LDLIBS)

$(libre2): FORCE
	( cd re2 && $(MAKE) )

clean:
	rm -f codesearch $(OBJECTS) $(DEPFILES)

$(OBJECTS): .config/CXX .config/CXXFLAGS

-include $(DEPFILES)

