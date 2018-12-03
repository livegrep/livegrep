/********************************************************************
 * livegrep -- re_width.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "re_width.h"

using namespace re2;

int WidthWalker::ShortVisit(Regexp *re, int parent_arg) {
    return 0;
}

int WidthWalker::PostVisit(Regexp *re, int parent_arg,
                           int pre_arg,
                           int *child_args, int nchild_args) {
    int width;
    switch (re->op()) {
    case kRegexpRepeat:
        width = child_args[0] * re->max();
        break;

    case kRegexpNoMatch:
    // These ops match the empty string:
    case kRegexpEmptyMatch:      // anywhere
    case kRegexpBeginLine:       // at beginning of line
    case kRegexpEndLine:         // at end of line
    case kRegexpBeginText:       // at beginning of text
    case kRegexpEndText:         // at end of text
    case kRegexpWordBoundary:    // at word boundary
    case kRegexpNoWordBoundary:  // not at word boundary
        width = 0;
        break;

    case kRegexpLiteral:
    case kRegexpAnyChar:
    case kRegexpAnyByte:
    case kRegexpCharClass:
        width = 1;
        break;

    case kRegexpLiteralString:
        width = re->nrunes();
        break;

    case kRegexpConcat:
        width = 0;
        for (int i = 0; i < nchild_args; i++)
            width += child_args[i];
        break;

    case kRegexpAlternate:
        width = 0;
        for (int i = 0; i < nchild_args; i++)
            width = std::max(width, child_args[i]);
        break;

    case kRegexpStar:
    case kRegexpPlus:
    case kRegexpQuest:
    case kRegexpCapture:
        width = child_args[0];
        break;

    default:
        abort();
    }

    return width;
}
