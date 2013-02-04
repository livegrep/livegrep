Code search experiments
=======================

"livegrep" is a tool, partially inspired by Google Code Search, for
interactive regex search of ~gigabyte-scale source repositories.

Dependencies
========

Livegrep has several dependencies, including:

 - [libgit2][lg]
 - [RE2][re2]
 - [gflags][gflags]


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
[lg-ppa]: https://launchpad.net/~nelhage/+archive/livegrep

The livegrep web interface is written in [node.js][node], and depends
on several [npm][npm] modules. They should all be listed in
`package.json`, can be installed via `npm install .`

[node]: http://nodejs.org/
[npm]: https://npmjs.org/

Components
==========

Run 'make to build the `codesearch` binary. This binary can be used by
hand, but is intended to be driven by the node.js helpers in the
`web/` directory.

The frontend programs can be configured in `web/config.local.js`. See
`web/config.js` for the configuration options. Once configuration is
established, `web/index.js` can be used to build a codesearch index.

The livegrep frontend then consists of two separate
programs. `web/cs_servers.js` runs a `codesearch` process, which it
communicates with via a pipe, and listens on a local
port. `web/web_server.js` runs the web server frontend, and
communicates with `cs_server` over a local socket.
