Livegrep [![Build Status](https://circleci.com/gh/livegrep/livegrep.png?branch=master)](https://circleci.com/gh/livegrep/livegrep)
========

Livegrep is a tool, partially inspired by Google Code Search, for
interactive regex search of ~gigabyte-scale source repositories. You
can see a running instance at
[http://livegrep.com/](http://livegrep.com).

Building
--------

livegrep builds using [bazel][bazel]. You will need to
[install bazel][bazel-install] version 0.12 or newer. livegrep vendors
and/or fetches all of its dependencies using `bazel`, and so should
only require a relatively recent C++ compiler to build.

Once you have those dependencies, you can build using

    bazel build //...

Note that the initial build will download around 100M of
dependencies. These will be cached once downloaded.

[bazel]: http://www.bazel.io/
[bazel-install]: http://www.bazel.io/docs/install.html

Invoking
--------

To run `livegrep`, you need to invoke both the `codesearch` backend
index/search process, and the `livegrep` web interface.

To run the sample web interface over livegrep itself, once you have
built both `codesearch` and `livegrep`:

In one terminal, start the `codesearch` server like so:

    bazel-bin/src/tools/codesearch -grpc localhost:9999 doc/examples/livegrep/index.json

In another, run livegrep:

    bazel-bin/cmd/livegrep/livegrep

In a browser, now visit
[http://localhost:8910/](http://localhost:8910/), and you should see a
working livegrep.

## Using Index Files

The `codesearch` binary is responsible for reading source code,
maintaining an index, and handling searches. `livegrep` is stateless
and relies only on the connection to `codesearch` over a TCP
connection.

By default, `codesearch` will build an in-memory index over the
repositories specified in its configuration file. You can, however,
also instruct it to save the index to a file on disk. This the dual
advantages of allowing indexes that are too large to fit in RAM, and
of allowing an index file to be reused. You instruct `codesearch` to
generate an index file via the `-dump_index` flag and to not launch
a search server via the `-index_only` flag:

    bazel-bin/src/tools/codesearch -index_only -dump_index livegrep.idx doc/examples/livegrep/index.json

Once `codeseach` has built the index, this index file can be used for
future runs. Index files are standalone, and you no longer need access
to the source code repositories, or even a configuration file, once an
index has been built. You can just launch a search server like so:

    bazel-bin/src/tools/codesearch -load_index livegrep.idx -grpc localhost:9999

## `livegrep`

The `livegrep` frontend expects an optional position argument
indicating a JSON configuration file; See
[doc/examples/livegrep/server.json][server.json] for an example, and
[server/config/config.go][config.go] for documentation of available
options.

By default, `livegrep` will connect to a single local codesearch
instance on port `9999`, and listen for HTTP connections on port
`8910`.

[server.json]: https://github.com/livegrep/livegrep/blob/master/doc/examples/livegrep/server.json
[config.go]: https://github.com/livegrep/livegrep/blob/master/server/config/config.go

## github integration

`livegrep` includes a helper driver, `livegrep-github-reindex`, which
can automatically update and index selected github repositories. To
download and index all of my repositories (except for forks), storing
the repos in `repos/` and writing `nelhage.idx`, you might run:

    bazel-bin/cmd/livegrep-github-reindex/livegrep-github-reindex -user=nelhage -forks=false -name=github.com/nelhage -out nelhage.idx

You can now use `nelhage.idx` as an argument to `codesearch
-load_index`.

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
