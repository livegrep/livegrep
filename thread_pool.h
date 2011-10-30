template <class J, class DoIt, class ThreadState>
class thread_pool {
public:
    thread_pool (int nthreads, DoIt& fn)
        : nthreads_(nthreads), fn_(fn) {
        threads_ = new pthread_t[nthreads_];
        for (int i = 0; i < nthreads; i++) {
            pthread_create(&threads_[i], NULL, worker, this);
        }
    }

    void queue(const J& job) {
        queue_.push(job);
    }

    ~thread_pool () {
        int i;
        for (i = 0; i < nthreads_; i++) {
            pthread_join(threads_[i], NULL);
        }
        delete threads_;
    }

protected:
    int nthreads_;
    pthread_t *threads_;
    thread_queue<J> queue_;
    DoIt& fn_;

    void worker() {
        ThreadState ts(fn_);
        while (true) {
            J job = queue_.pop();
            if (fn_(ts, job))
                break;
        }
    }

    static void *worker(void *arg) {
        thread_pool *pool = static_cast<thread_pool*>(arg);
        pool->worker();
        return NULL;
    }
};
