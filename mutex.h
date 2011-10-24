#include <pthread.h>

class cond_var;

class mutex {
public:
    mutex () {
        pthread_mutex_init(&mutex_, NULL);
    }

    ~mutex () {
        pthread_mutex_destroy(&mutex_);
    }

    void lock () {
        pthread_mutex_lock(&mutex_);
    }

    void unlock () {
        pthread_mutex_unlock(&mutex_);
    }
protected:
    mutex(const mutex&);
    mutex operator=(const mutex&);
    pthread_mutex_t mutex_;

    friend class cond_var;
};

class cond_var {
public:
    cond_var() {
        pthread_cond_init(&cond_, NULL);
    }

    ~cond_var() {
        pthread_cond_destroy(&cond_);
    }

    void wait(mutex *mutex) {
        pthread_cond_wait(&cond_, &mutex->mutex_);
    }

    void signal() {
        pthread_cond_signal(&cond_);
    }

    void broadcast() {
        pthread_cond_broadcast(&cond_);
    }
protected:
    pthread_cond_t cond_;
};

class mutex_locker {
public:
    mutex_locker(mutex& mutex)
        : mutex_(mutex) {
        mutex_.lock();
    }

    ~mutex_locker() {
        mutex_.unlock();
    }
 protected:

    mutex_locker(const mutex_locker& rhs);
    mutex_locker operator=(const mutex_locker &rhs);

    mutex &mutex_;
};
