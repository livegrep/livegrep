#ifndef CODESEARCH_THREADSAFE_PROGRESS_INDICATOR
#define CODESEARCH_THREADSAFE_PROGRESS_INDICATOR

#include <atomic>
#include <mutex>
#include <iostream>
#include <cmath>

class threadsafe_progress_indicator {
    public:
        threadsafe_progress_indicator(float work_todo, 
                                      const char * prefix, const char * done_suffix_)
        : work_todo_(work_todo), work_done_(0), prefix_(prefix), done_suffix_(done_suffix_), wrote_done_suffix_(false) {
        }

        void tick(std::ostream &os = std::cout) {
            // raise an error if we're ticking more than done?
            std::lock_guard<std::mutex> guard(mutex_);
            work_done_++;

            if (work_todo_ == work_done_ && wrote_done_suffix_) return;

            os << "\r" << std::flush;

            os << prefix_;

            float percent_done = std::round((work_done_ / work_todo_) * 100);

            os << " " << percent_done << "% - [" 
                << work_done_ << "/" << work_todo_ << "]";

            if (work_todo_ == work_done_) {
                os << " " << done_suffix_ << "\n";
                wrote_done_suffix_ = true;
                os << std::flush;
            }
        }

    private:
        std::mutex mutex_;
        float work_todo_;
        float work_done_;
        const char * prefix_;
        const char * done_suffix_;
        bool wrote_done_suffix_;
};

#endif
