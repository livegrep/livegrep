include Makefile.lib
-include Makefile.config

ifeq ($(libre2),)
libre2=/usr
endif

extradirs=$(filter-out /usr,$(sort $(libgit2) $(gflags) $(libre2)))

override CPPFLAGS += $(patsubst %,-I%/include, $(extradirs))
override LDFLAGS += $(patsubst %, -L%/lib, $(extradirs))
override LDFLAGS += $(patsubst %, -Wl$(comma)-R%/lib, $(extradirs))

override CXXFLAGS+=-g -std=c++0x -Wall -Werror -Wno-sign-compare -pthread
override LDFLAGS+=-pthread
LDLIBS=-lgit2 -ljson -lgflags $(libre2)/lib/libre2.a

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

DIRS := src src/lib src/3party
override CPPFLAGS += $(patsubst %,-I%, $(DIRS))
OBJECTS :=
include $(patsubst %, %/Makefrag, $(DIRS))
DEPFILES := $(foreach obj,$(OBJECTS), $(dir $(obj)).$(notdir $(obj:.o=)).d)

all: codesearch $(DEPFILES)

codesearch: $(OBJECTS) $(MAKEVARS)/LDFLAGS
	$(CXX) -o $@ $(LDFLAGS) $(filter-out $(MAKEVARS)/%,$^) $(LDLIBS)

clean:
	rm -f codesearch $(OBJECTS) $(DEPFILES)

$(OBJECTS): $(MAKEVARS)/CXX $(MAKEVARS)/CXXFLAGS

-include $(DEPFILES)

