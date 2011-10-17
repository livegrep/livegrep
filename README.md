Code search experiments
=======================

This is an experiment, inspired by Google Code Search, in large-scale regex
search across code bases.

Building
========

The build requires the [libgit2][lg] and [RE2][re2] libraries. If they are
installed in non-default paths, create a `Makefile.config` file in the top level
of the repository the defines `re2` and/or `libgit2` variables pointing at the
install prefix for those libraries. For example, I have:

    libgit2=/home/nelhage/sw/libgit2
    re2=/home/nelhage/sw/re2

[libgit2]: http://libgit2.github.com/
[re2]: http://code.google.com/p/re2/

Usage
=====


Usage is:

    ./codesearch COMMIT [COMMIT COMMIT COMMIT...]

where `COMMIT` is either a sha1 or a fully-qualified ref
(e.g. `refs/heads/master`). `git rev-parse` syntax is not supported -- use
backticks if you need it :)

Some progress information will be printed, and then you will be given a `regex>
` prompt at which you can type a regular expression to get matches.
