#include <pthread.h>

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

    operator pthread_mutex_t* (void) {
        return &mutex_;
    }
private:
    mutex(const mutex&);
    mutex operator=(const mutex&);
    pthread_mutex_t mutex_;
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
