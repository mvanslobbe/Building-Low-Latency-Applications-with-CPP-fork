#pragma once

#include <iostream>
#include <vector>
#include <atomic>
#include <bit>

#include "macros.h"

namespace Common {
  template<typename T>
  class LFQueue final {
  public:
    explicit LFQueue(std::size_t num_elems) :
        store_(num_elems, T()) /* pre-allocation of vector storage. */, index_mask_(num_elems - 1) {
      ASSERT(std::has_single_bit(num_elems), "LFQueue size must be a power of two, got:" + std::to_string(num_elems));
    }

    auto getNextToWriteTo() noexcept {
      return &store_[next_write_index_.load(std::memory_order_relaxed) & index_mask_];
    }

    /// Only the writer thread modifies next_write_index_, so it can read its own index relaxed.
    /// The release store publishes the element written before it to the reader.
    auto updateWriteIndex() noexcept {
      next_write_index_.store(next_write_index_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    /// The acquire load pairs with the release store in updateWriteIndex(), so the element is fully visible.
    auto getNextToRead() const noexcept -> const T * {
      const auto read_index = next_read_index_.load(std::memory_order_relaxed);
      return (read_index != next_write_index_.load(std::memory_order_acquire) ? &store_[read_index & index_mask_] : nullptr);
    }

    /// Only the reader thread modifies next_read_index_.
    /// The release store tells the writer the slot is no longer being read.
    auto updateReadIndex() noexcept {
      // Build the message only on failure: ASSERT is a function, so its message would be built on every pop.
      // See https://github.com/PacktPublishing/Building-Low-Latency-Applications-with-CPP/pull/11
      if (UNLIKELY(size() == 0))
        ASSERT(false, "Read an invalid element in:" + std::to_string(pthread_self()));
      next_read_index_.store(next_read_index_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    /// Load the read index first: the write index never falls behind it, so the difference cannot underflow.
    auto size() const noexcept {
      const auto read_index = next_read_index_.load(std::memory_order_acquire);
      return next_write_index_.load(std::memory_order_acquire) - read_index;
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

    /// store_.size() - 1. The size is a power of two, so index & mask wraps an index without a division.
    const size_t index_mask_;

    /// Atomic trackers for next index to write new data to and read new data from.
    /// Each lives on its own cache line so the writer and the reader do not invalidate each other's line.
    /// The indices only ever increase and are wrapped on access with index_mask_, so size() is their difference
    /// and no counter has to be shared between the two threads.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> next_write_index_ = {0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> next_read_index_ = {0};
  };
}
