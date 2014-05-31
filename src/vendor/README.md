# Vendored 3rd-party repositories

`vendor/` contains third-party repositories that have been copied, in
their entirety, into the livegrep repository for
ease-of-building. Nothing in the subdirectories of this repository
should be edited other than by direct imports of upstream.

Vendored repositories:

- re2: Russ Cox's [re2](https://code.google.com/p/re2/) library for
  efficient regex matching. Vendored because we depend on internal
  headers that are not part of a normal re2 install, and because re2
  packages are not widely available, regardless.

- utf8cpp: A
  [library for portable C++ UTF-8 handling](http://utfcpp.sourceforge.net/).

- gtest: The [Google C++ Test framework](https://code.google.com/p/googletest/)
