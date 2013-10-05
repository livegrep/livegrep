Livegrep
========

"livegrep" is a tool, partially inspired by Google Code Search, for
interactive regex search of ~gigabyte-scale source repositories.

Dependencies
------------

Livegrep has several dependencies, including:

 - [libgit2][libgit2]
 - [RE2][re2]
 - [gflags][gflags]
 - [libjson][libjson]


I have packaged these for Ubuntu in my [ppa][lg-ppa], or you can
install them from source or from other packages. If they are installed
in non-default paths, create a `Makefile.config` file in the top level
of the repository the defines `re2` and/or `libgit2` variables
pointing at the install prefix for those libraries. For example, you
might have:

    libgit2=/home/nelhage/sw/libgit2
    re2=/home/nelhage/sw/re2

[libgit2]: http://libgit2.github.com/
[re2]: http://code.google.com/p/re2/
[gflags]: https://code.google.com/p/gflags/?redir=1
[libjson]: http://oss.metaparadigm.com/json-c/
[lg-ppa]: https://launchpad.net/~nelhage/+archive/livegrep

The livegrep web interface is written in [node.js][node], and depends
on several [npm][npm] modules. They should all be listed in
`package.json`, can be installed via `npm install .`

[node]: http://nodejs.org/
[npm]: https://npmjs.org/

Livegrep has only been tested on Linux x86_64 systems. It is unlikely
to work well (or potentially at all) on 32-bit systems because of the
large virtual memory footprint it requires.

Components
----------

Run `make` to build the `codesearch` binary. This binary can be used
by hand, but is intended to be driven by the node.js helpers in the
`bin/` directory.

The frontend programs can be configured in `js/config.local.js`. See
`js/config.js` for the configuration options. Once configuration is
established, `bin/index.js` can be used to build a codesearch index.

The livegrep frontend then consists of two separate
programs. `bin/cs_servers.js` runs a `codesearch` process, which it
communicates with via a pipe, and listens on a local
port. `bin/web_server.js` runs the web server frontend, and
communicates with `cs_server` over a local socket.

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
