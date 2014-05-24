-include Makefile.config

extradirs=$(filter-out /usr,$(sort $(libgit2) $(gflags)))

override CPPFLAGS += $(patsubst %,-I%/include, $(extradirs)) -I src/vendor/re2/ -I src/vendor/utf8cpp/source
override LDFLAGS += $(patsubst %, -L%/lib, $(extradirs))
override LDFLAGS += $(patsubst %, -Wl$(comma)-R%/lib, $(extradirs))

override CXXFLAGS+=-g -std=c++0x -Wall -Werror -Wno-sign-compare -pthread
override LDFLAGS+=-pthread
LDLIBS=-lgit2 -ljson -lgflags $(re2) -lz -lssl -lcrypto -ldl -lboost_system -lboost_filesystem

re2 := src/vendor/re2/obj/libre2.a

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

DIRS := src src/lib src/tools
include Makefile.lib

$(TOOLS): $(re2)

$(re2): FORCE
	$(MAKE) -C src/vendor/re2 obj/libre2.a
