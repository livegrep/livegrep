Livegrep [![Build Status](https://circleci.com/gh/livegrep/livegrep.png?branch=master)](https://circleci.com/gh/livegrep/livegrep)
========

Livegrep is a tool, partially inspired by Google Code Search, for
interactive regex search of ~gigabyte-scale source repositories. You
can see a running instance at
[http://livegrep.com/](http://livegrep.com).

Building
--------

livegrep builds using [bazel][bazel]. You will need to
[install][bazel-install] a fairly recent version: as of this writing
we test on bazel 4.0.0.

livegrep vendors and/or fetches all of its dependencies using `bazel`,
and so should only require a relatively recent C++ compiler to build.

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
also instruct it to save the index to a file on disk. This has the dual
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

The schema for the `codesearch` configuration file defined using
protobuf in [src/proto/config.proto](src/proto/config.proto).

## `livegrep`

The `livegrep` frontend accepts an optional position argument
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

## Local repository browser
`livegrep` provides the ability to view source files directly in `livegrep`, as
an alternative to linking files to external viewers. This was initially implemented
by @jboning [here](https://github.com/livegrep/livegrep/pull/70). There are
a few ways to enable this. The most important steps are to
1. Generate a config file that `livegrep` can use to figure out where your
   source files are (locally).
2. Pass this config file as an argument to the frontend (`-index-config`)

### Generating index manually

See [doc/examples/livegrep/server.json](doc/examples/livegrep/server.json) for an
example config file, and [server/config/config.go](server/config/config.go) for documentation on available options. To enable the file viewer, you must include an [`IndexConfig`](server/config/config.go#L61) block inside of the config file. An example `IndexConfig` block can be seen at [doc/examples/livegrep/index.json](doc/examples/livegrep/index.json). 

*Tip: For each repository included in your `IndexConfig`, make sure to include `metadata.url_pattern` if you would like the file viewer to be able to link out to the external host. You'll see a warning in your browser console if you don't do this.*

### Generating index with `livegrep-github-reindex`
If you are already using the `livegrep-github-reindex` tool, an IndexConfig index file is generated for you, by default named "livegrep.json".

Run the indexer
```
bazel-bin/cmd/livegrep-github-reindex/livegrep-github-reindex_/livegrep-github-reindex -user=xvandish -forks=false -name=github.com/xvandish -out xvandish.idx ```
```

The indexer will have done these main things:
1. Clone (or update) all repositories for `user=xvandish` to/in `repos/xvandish`
2. Create an IndexConfig file - `repos/livegrep.json`
3. Create a code index, this is whats used to search - `./xvandish.idx`

Here's an abbreviated version of what your directory might look like after running the indexer.
```
livegrep
│   xvandish.idx
└───repos
│   │   livegrep.json
│   └───xvandish
│       └───repo1
│       └───repo2
│       └───repo3
```

### Using your generated index
Now that you generated an index file, it's time to run livegrep with it.

Run the backend:
```
bazel-bin/src/tools/codesearch -load_index xvandish.idx -grpc localhost:9999
```

Run the frontend in another shell instance with the path to the index file located at `repos/livegrep.json`.
```
bazel-bin/cmd/livegrep/livegrep_/livegrep -index-config ./repos/livegrep.json
```
In a browser, now visit `http://localhost:8910` and you should see a working
livegrep. Search for something, and once you get a result, click on the file
name or a line number. You should now be taken to the file browser!

Docker images
-------------

I build [docker images][docker] for livegrep out of the
[livegrep.com](https://github.com/livegrep/livegrep.com) repository,
based on build images created by this repository's CI. They should be
generally usable. For instance, to build+run a livegrep index of this
repository, you could run:

```
docker run -v $(pwd):/data livegrep/indexer /livegrep/bin/livegrep-github-reindex -repo livegrep/livegrep -http -dir /data
docker network create livegrep
docker run -d --rm -v $(pwd):/data --network livegrep --name livegrep-backend livegrep/base /livegrep/bin/codesearch -load_index /data/livegrep.idx -grpc 0.0.0.0:9999
docker run -d --rm --network livegrep --publish 8910:8910 livegrep/base /livegrep/bin/livegrep -docroot /livegrep/web -listen=0.0.0.0:8910 --connect livegrep-backend:9999
```

And then access http://localhost:8910/

You can also find the [docker-compose config powering
livegrep.com][docker-compose] in that same repository.

[docker]: https://hub.docker.com/u/livegrep
[docker-compose]: https://github.com/livegrep/livegrep.com/tree/master/compose

Resource Usage
--------------

livegrep builds an index file of your source code, and then works
entirely out of that index, with no further access to the original git
repositories.

The index file will vary somewhat in size, but will usually be 3-5x
the size of the indexed text. `livegrep` memory-maps the index file
into RAM, so it can work out of index files larger than (available)
RAM, but will perform better if the file can be loaded entirely into
memory. Barring that, keeping the disk on fast SSDs is recommended for
optimal performance.


LICENSE
-------

Livegrep is open source. See [COPYING](COPYING) for more information.
