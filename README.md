Livegrep
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
 - [RE2][re2]
 - [gflags][gflags]
 - [libjson][libjson]

On a sufficiently recent Ubuntu, these are all available via `apt-get`:

    sudo apt-get install libgit2-dev re2-dev libgflags-dev libjson0-dev

I have also made packages available in a [PPA](lg-ppa), but they are
largely unmaintained since I no longer deploy livegrep on any older
distributions.

[libgit2]: http://libgit2.github.com/
[re2]: http://code.google.com/p/re2/
[gflags]: https://code.google.com/p/gflags/?redir=1
[libjson]: http://oss.metaparadigm.com/json-c/
[lg-ppa]: https://launchpad.net/~nelhage/+archive/livegrep

Once all the dependencies are installed, a simple `make` should build
the `codesearch` binary.

### `livegrep` -- the web interface

    go install github.com/nelhage/livegrep/livegrep

should suffice to install the livegrep web UI into `$GOPATH/bin`

### `lg` -- the CLI

Similarly,

    go install github.com/nelhage/livegrep/lg

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
