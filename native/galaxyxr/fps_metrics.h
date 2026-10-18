// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// Successful app projection submissions per monotonic second. This does not
// measure GPU completion, distinct rendered images, presentation, or display Hz.
class GxrFpsMetrics {
public:
    void reset() { started_ = last_ = 0; count_ = 0; fps_ = 0; valid_ = active_ = false; }
    bool sample(uint64_t nowNs, bool acceptedProjection) {
        if (!acceptedProjection) return false;
        if (!active_ || nowNs <= last_ || nowNs - last_ > 2000000000ULL) {
            started_ = last_ = nowNs;
            count_ = 0;
            active_ = true;
            valid_ = false;
            return false;
        }
        last_ = nowNs;
        ++count_;
        const uint64_t elapsed = nowNs - started_;
        if (elapsed < 1000000000ULL) return false;
        fps_ = static_cast<double>(count_) * 1e9 / static_cast<double>(elapsed);
        valid_ = true;
        started_ = nowNs;
        count_ = 0;
        return true;
    }
    bool valid() const { return valid_; }
    double fps() const { return fps_; }
private:
    uint64_t started_ = 0, last_ = 0, count_ = 0;
    double fps_ = 0;
    bool valid_ = false, active_ = false;
};

// Wait(N+1) may finish while frame N is rendering. Associate each wait with
// its matching begin instead of reading a mutable "latest shouldRender" bit.
// Overflow is invalid call cadence: fail closed until lifecycle reset.
class GxrFpsFrameLatch {
public:
    void reset() { head_ = size_ = 0; active_ = render_ = overflow_ = false; }
    void wait(bool shouldRender) {
        if (overflow_) return;
        if (size_ == Capacity) { reset(); overflow_ = true; return; }
        queued_[(head_ + size_) % Capacity] = shouldRender;
        ++size_;
    }
    void begin() {
        active_ = !overflow_ && size_ > 0;
        render_ = active_ && queued_[head_];
        if (active_) { head_ = (head_ + 1) % Capacity; --size_; }
    }
    bool finish() { bool render = active_ && render_; active_ = false; return render; }
private:
    static constexpr unsigned Capacity = 8;
    bool queued_[Capacity]{};
    unsigned head_ = 0, size_ = 0;
    bool active_ = false, render_ = false, overflow_ = false;
};
