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

$(re2): FORCE
	$(MAKE) -C src/vendor/re2 obj/libre2.a

test: FORCE godep test/codesearch_test
	test/codesearch_test
	go test github.com/nelhage/livegrep/client github.com/nelhage/livegrep/server

ifeq ($(GOPATH),)
override GOPATH = $(CURDIR)/.gopath
export GOPATH
gopath: FORCE
	mkdir -p $(GOPATH)/src/github.com/nelhage/
	ln -nsf $(CURDIR) $(GOPATH)/src/github.com/nelhage/livegrep
else
gopath: FORCE
endif

godep: gopath FORCE
	go get github.com/nelhage/livegrep/livegrep github.com/nelhage/livegrep/lg

bin/lg: godep FORCE
	go build -o bin/lg github.com/nelhage/livegrep/lg

bin/livegrep: godep FORCE
	go build -o bin/livegrep github.com/nelhage/livegrep/livegrep

EXTRA_TARGETS := godep bin/lg bin/livegrep
EXTRA_CLEAN := bin/ .gopath/

DIRS := src src/lib src/tools test
include Makefile.lib

$(TOOLS): $(re2)
