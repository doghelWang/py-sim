#pragma once

#include <vector>
#include <mutex>
#include <atomic>

namespace amr {

template <typename T>
class DoubleBuffer {
public:
    // Writer appends to back buffer
    void Push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        back_buffer_.push_back(item);
    }
    
    // Writer appends multiple
    void PushBatch(const std::vector<T>& items) {
        std::lock_guard<std::mutex> lock(mtx_);
        back_buffer_.insert(back_buffer_.end(), items.begin(), items.end());
    }

    // Reader swaps and takes ownership of data
    std::vector<T> Swap() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<T> temp = std::move(front_buffer_); // move old front (should be empty if consumed)
        front_buffer_ = std::move(back_buffer_);
        back_buffer_ = std::move(temp); // reclaim capacity
        back_buffer_.clear(); // clear new back
        return std::move(front_buffer_); // return new front
    }

    // Check if new data is available without full lock (hint)
    bool HasData() const {
        // Not perfectly safe without lock but good for "should I swap?" check
        // std::lock_guard<std::mutex> lock(mtx_); // Too heavy?
        return true; // Always swap for simplicity in render loop? or add atomic count
    }

private:
    std::vector<T> back_buffer_;
    std::vector<T> front_buffer_;
    mutable std::mutex mtx_;
};

} // namespace amr
