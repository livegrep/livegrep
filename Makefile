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
LDLIBS=-lgit2 -ljson -lgflags $(libre2)/lib/libre2.a -lcrypto

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

DIRS := src src/lib src/3party src/tools
override CPPFLAGS += $(patsubst %,-I%, $(DIRS))
SRC :=
TOOLS :=
include $(patsubst %, %/Makefrag, $(DIRS))
LIBOBJS := $(foreach src,$(SRC),$(basename $(src)).o)
OBJECTS := $(LIBOBJS) $(patsubst %,src/tools/%.o, $(TOOLS))
DEPFILES := $(foreach obj,$(OBJECTS),$(dir $(obj)).$(notdir $(obj:.o=)).d)

all: $(TOOLS) $(DEPFILES)

define build_tool
$(1): $$(LIBOBJS) src/tools/$(1).o $$(MAKEVARS)/LDFLAGS
	$$(CXX) -o $$@ $$(LDFLAGS) $$(filter-out $$(MAKEVARS)/%,$$^) $$(LDLIBS)
endef
$(foreach tool,$(TOOLS),$(eval $(call build_tool,$(tool))))

clean:
	rm -f $(TOOLS) $(OBJECTS) $(DEPFILES)

$(OBJECTS): $(MAKEVARS)/CXX $(MAKEVARS)/CXXFLAGS

-include $(DEPFILES)

