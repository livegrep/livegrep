#include <list>

#include "mutex.h"

template <class T>
class thread_queue {
public:
    thread_queue () {
        pthread_cond_init(&cond_, NULL);
    }

    void push(const T& val) {
        mutex_locker locked(mutex_);
        queue_.push_back(val);
        pthread_cond_signal(&cond_);
    }

    T pop() {
        mutex_locker locked(mutex_);
        while (queue_.empty())
            pthread_cond_wait(&cond_, mutex_);
        T rv = queue_.front();
        queue_.pop_front();
        return rv;
    }

    bool try_pop(T &ret) {
        mutex_locker locked(mutex_);
        if (queue_.empty())
            return false;
        ret = queue_.front();
        queue_.pop_front();
    }

    ~thread_queue() {
        pthread_cond_destroy(&cond_);
    }
 protected:
    thread_queue(const thread_queue&);
    thread_queue operator=(const thread_queue &);
    mutex mutex_;
    pthread_cond_t cond_;
    std::list<T> queue_;
};
