#pragma once

#include <iostream>
#include <vector>
#include <atomic>

#include "macros.h"

namespace Common {
  template<typename T>
  class LFQueue final {
  public:
    explicit LFQueue(std::size_t num_elems) :
        store_(num_elems, T()) /* pre-allocation of vector storage. */ {
    }

    auto getNextToWriteTo() noexcept {
      return &store_[next_write_index_];
    }

    auto updateWriteIndex() noexcept {
      next_write_index_ = (next_write_index_ + 1) % store_.size();
      num_elements_++;
    }

    auto getNextToRead() const noexcept -> const T * {
      return (size() ? &store_[next_read_index_] : nullptr);
    }

    auto updateReadIndex() noexcept {
      next_read_index_ = (next_read_index_ + 1) % store_.size(); // wrap around at the end of container size.
      // Build the message only on failure: ASSERT is a function, so its message would be built on every pop.
      // See https://github.com/PacktPublishing/Building-Low-Latency-Applications-with-CPP/pull/11
      if (UNLIKELY(num_elements_ == 0))
        ASSERT(false, "Read an invalid element in:" + std::to_string(pthread_self()));
      num_elements_--;
    }

    auto size() const noexcept {
      return num_elements_.load();
    }

    /// Deleted default, copy & move constructors and assignment-operators.
    LFQueue() = delete;

    LFQueue(const LFQueue &) = delete;

    LFQueue(const LFQueue &&) = delete;

    LFQueue &operator=(const LFQueue &) = delete;

    LFQueue &operator=(const LFQueue &&) = delete;

  private:
    /// Larger than a 64-byte cache line because Intel's spatial prefetcher pulls lines in adjacent pairs.
    static constexpr size_t CACHE_LINE_SIZE = 128;

    /// Underlying container of data accessed in FIFO order.
    /// Read-only after construction, so it must not share a cache line with the indices written below.
    std::vector<T> store_;

    /// Atomic trackers for next index to write new data to and read new data from.
    /// Each lives on its own cache line so the writer and the reader do not invalidate each other's line.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> next_write_index_ = {0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> next_read_index_ = {0};

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> num_elements_ = {0};
  };
}
