#include <sys/time.h>
#include <assert.h>

class timer {
public:
    timer(bool startnow = true)
        : running_(false), elapsed_({0,0}){
        if (startnow)
            start();
    }

    void start() {
        assert(!running_);
        running_ = true;
        gettimeofday(&start_, NULL);
    }

    void pause() {
        struct timeval now, delta;
        assert(running_);
        running_ = false;
        gettimeofday(&now, NULL);
        timeval_subtract(&delta, &now, &start_);
        timeval_add(&elapsed_, &delta, &elapsed_);
    }

    void reset() {
        running_ = false;
        elapsed_ = (struct timeval){0,0};
    }

    void add(timer &other) {
        assert(!running_);
        struct timeval elapsed = other.elapsed();
        timeval_add(&elapsed_, &elapsed_, &elapsed);
    }

    struct timeval elapsed() {
        if (running_) {
            struct timeval now, delta;
            gettimeofday(&now, NULL);
            timeval_subtract(&delta, &now, &start_);
            timeval_add(&elapsed_, &delta, &elapsed_);
            start_ = now;
        }
        return elapsed_;
    }

protected:
    bool running_;
    struct timeval start_;
    struct timeval elapsed_;

    timer(const timer& rhs);
    timer operator=(const timer& rhs);

    /* Subtract the `struct timeval' values X and Y,
       storing the result in RESULT.
       Return 1 if the difference is negative, otherwise 0.  */
    static int
    timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
    {
        /* Perform the carry for the later subtraction by updating y. */
        if (x->tv_usec < y->tv_usec) {
            int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
            y->tv_usec -= 1000000 * nsec;
            y->tv_sec += nsec;
        }
        if (x->tv_usec - y->tv_usec > 1000000) {
            int nsec = (x->tv_usec - y->tv_usec) / 1000000;
            y->tv_usec += 1000000 * nsec;
            y->tv_sec -= nsec;
        }

        /* Compute the time remaining to wait.
           tv_usec is certainly positive. */
        result->tv_sec = x->tv_sec - y->tv_sec;
        result->tv_usec = x->tv_usec - y->tv_usec;

        /* Return 1 if result is negative. */
        return x->tv_sec < y->tv_sec;
    }

    static void
    timeval_add(struct timeval *res, const struct timeval *x,
                const struct timeval *y)
    {
        res->tv_sec = x->tv_sec + y->tv_sec;
        res->tv_usec = x->tv_usec + y->tv_usec;

        while (res->tv_usec > 1000000) {
            res->tv_usec -= 1000000;
            res->tv_sec++;
        }
    }
};

class run_timer {
public:
    run_timer(timer& timer)
        : timer_(timer) {
        timer_.start();
    }
    ~run_timer() {
        timer_.pause();
    }
protected:
    timer &timer_;
};
