/********************************************************************
 * livegrep -- smart_git.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_SMART_GIT_H
#define CODESEARCH_SMART_GIT_H

#include "git2.h"

class smart_object_base {
public:
    smart_object_base() : obj_(0) {
    };

    operator git_object* () {
        return obj_;
    }

    operator git_object** () {
        return &obj_;
    }

    ~smart_object_base() {
        if (obj_)
            git_object_free(obj_);
    }

    git_object *release() {
        git_object *o = obj_;
        obj_ = 0;
        return o;
    }

protected:
    smart_object_base(const smart_object_base& rhs) {
    }
    git_object *obj_;
};

template <class T>
class object_traits { const static git_otype type; };

template <>
struct object_traits<git_tree> { const static git_otype git_type = GIT_OBJ_TREE; };
template <>
struct object_traits<git_commit> { const static git_otype git_type = GIT_OBJ_COMMIT; };
template <>
struct object_traits<git_blob> { const static git_otype git_type = GIT_OBJ_BLOB; };
template <>
struct object_traits<git_tag> { const static git_otype git_type = GIT_OBJ_TAG; };

template <>
struct object_traits<const git_tree> { const static git_otype git_type = GIT_OBJ_TREE; };
template <>
struct object_traits<const git_commit> { const static git_otype git_type = GIT_OBJ_COMMIT; };
template <>
struct object_traits<const git_blob> { const static git_otype git_type = GIT_OBJ_BLOB; };
template <>
struct object_traits<const git_tag> { const static git_otype git_type = GIT_OBJ_TAG; };


template <class T>
class smart_object : public smart_object_base {
public:
    operator T* () {
        assert(obj_);
        assert(git_object_type(obj_) == object_traits<T>::git_type);
        return reinterpret_cast<T*>(obj_);
    }
    operator T** () {
        assert(obj_ == 0);
        return reinterpret_cast<T**>(&obj_);
    }

    T *release() {
        T *o = this;
        obj_ = 0;
        return o;
    }

    smart_object<T>& operator=(git_object *rhs) {
        assert(obj_ == 0);
        assert(git_object_type(rhs) == object_traits<T>::git_type);
        obj_ = rhs;
        return *this;
    }
};

template <>
class smart_object <git_object> : public smart_object_base {
public:
    template <class O>
    operator O* () {
        assert(git_object_type(obj_) == object_traits<O>::git_type);
        return reinterpret_cast<O*>(obj_);
    }

    template <class O>
    operator O** () {
        assert(object_traits<O>::git_type);
        return reinterpret_cast<O**>(&obj_);
    }

};

#endif /* !defined(CODESEARCH_SMART_GIT_H) */
