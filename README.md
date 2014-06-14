Livegrep [![Build Status](https://travis-ci.org/nelhage/livegrep.svg?branch=master)](https://travis-ci.org/nelhage/livegrep)
========

Livegrep is a tool, partially inspired by Google Code Search, for
interactive regex search of ~gigabyte-scale source repositories. You
can see a running instance at
[http://livegrep.com/](http://livegrep.com).

Building
--------

This respository contains three separate components: The indexing and
search backend (written in C++), the web interface (server in golang,
UI, obviously, in Javascript), and a CLI that talks to the web
interface. These each need to be built separately.

### `codesearch` -- the search backend

The C++ backend had a number of dependencies, including:

 - [libgit2][libgit2]
 - [gflags][gflags]
 - [libjson][libjson]
 - [boost][boost] (in particular, `boost::system` and `boost:filesystem`)

On a sufficiently recent Ubuntu, these are all available via `apt-get`:

    sudo apt-get install libgflags-dev libgit2-dev libjson0-dev libboost-system-dev libboost-filesystem-dev libsparsehash-dev

I have also made packages available in a [PPA][lg-ppa], but they are
largely unmaintained since I no longer deploy livegrep on any older
distributions.

[libgit2]: http://libgit2.github.com/
[gflags]: https://code.google.com/p/gflags/?redir=1
[libjson]: http://oss.metaparadigm.com/json-c/
[boost]: http://www.boost.org/
[lg-ppa]: https://launchpad.net/~nelhage/+archive/livegrep

Once all the dependencies are installed, a simple `make` should build
all of the binaries into the `bin/` directory.

Invoking
--------

## `codesearch`

The simplest way to invoke livegrep is to use the `codeseach` binary
directly, in "CLI" mode, for interactive use on the command line. To
start searching a repository:

    bin/codesearch -cli .

In order to index a repository once and save the index for fast
startup later, you can use the `-load_index` and `-dump_index` flags.

    bin/codesearch -cli -dump_index livegrep.idx .

Will index this repository and save the index into `livegrep.idx`. You
can then re-use that index file later:

    bin/codesearch -cli -load_index livegrep.idx

With `-load_index`, only the index file is looked at -- the original
git repositories need not even be present on the filesystem, and any
positional arguments to the command are discarded.

For programmatic use, leaving off `-cli` runs in a JSON interface
mode. In this mode, a single position argument is expected, which is a
JSON configuration file specifying which repositories and revisions to
index. You can find a trivial example at
[doc/examples/livegrep/index.json][index.json].

You can also provide `-listen proto://host:port` to make `codesearch`
start a server and listen on a port for incoming connections. This is
needed to run `codesearch` as a backend for the `livegrep` frontend.

[index.json]: https://github.com/nelhage/livegrep/blob/master/doc/examples/livegrep/index.json

## `livegrep`

In order to run the `livegrep` web interface, you need one or more
`codesearch` backends listening on TCP ports for `livegrep` to connect
to. `livegrep` expects a JSON configuration file as a single
positional argument; See
[doc/examples/livegrep/server.json][server.json] for an example, and
[server/config/config.go][config.go] for documentation of available
options.

Livegrep uses [glog][glog] for logging. You can consult its
documentation for the full set of logging options. During development,
`-logtostderr` will send all logs to standard out for easy viewing.

[server.json]: https://github.com/nelhage/livegrep/blob/master/doc/examples/livegrep/server.json
[config.go]: https://github.com/nelhage/livegrep/blob/master/server/config/config.go
[glog]: https://github.com/golang/glog

## Example

To run the sample web interface over livegrep itself, once you have
built both `codesearch` and `livegrep`:

In one terminal, start the `codesearch` server like so:

    bin/codesearch -listen tcp://localhost:9999 doc/examples/livegrep/index.json

In another, run livegrep:

    bin/livegrep -logtostderr doc/examples/livegrep/server.json

In a browser, now visit
[http://localhost:8910/](http://localhost:8910/), and you should see a
working livegrep.

Resource Usage
--------------

livegrep builds an index file of your source code, and then works
entirely out of that index, with no further access to the original git
repositories.

In general, the index file will be approximately the same size as the
original source code. livegrep memory-maps the index file into RAM, so
it should be able to work out of index files larger than (available)
RAM, but will perform much better if the file can be loaded entirely
into memory.


LICENSE
-------

Livegrep is open source. See COPYING for more information.
