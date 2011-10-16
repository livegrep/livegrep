#include <git2.h>

class smart_object_base {
public:
    smart_object_base() : obj_(0) {
    };

    operator git_object** () {
        return &obj_;
    }

    ~smart_object_base() {
        if (obj_)
            git_object_close(obj_);
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
};

template <>
class smart_object <git_object> : public smart_object_base {
public:
    template <class O>
    operator O* () {
        assert(git_object_type(obj_) == object_traits<O>::git_type);
        return reinterpret_cast<O*>(obj_);
    }
};
