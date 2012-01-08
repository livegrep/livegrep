#ifndef CODESEARCH_RE_WIDTH_H
#define CODESEARCH_RE_WIDTH_H

#include <re2/regexp.h>
#include "re2/walker-inl.h"

using re2::Regexp;

class WidthWalker : public Regexp::Walker<int> {
public:
  virtual int PostVisit(
      Regexp* re, int parent_arg,
      int pre_arg,
      int *child_args, int nchild_args);

  virtual int ShortVisit(
      Regexp* re,
      int parent_arg);
};

#endif
