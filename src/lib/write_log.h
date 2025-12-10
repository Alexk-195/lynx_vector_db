/**
 * @file write_log.h
 * @brief Write log for non-blocking index maintenance
 *
 * This file contains the WriteLog struct used to capture write operations
 * during index maintenance, enabling non-blocking optimization via a
 * clone-optimize-replay-swap pattern.
 */

#ifndef LYNX_WRITE_LOG_H
#define LYNX_WRITE_LOG_H

#include "../include/lynx/lynx.h"
#include "hnsw_index.h"
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <span>

namespace lynx {

/**
 * @brief Write log for tracking operations during index maintenance.
 *
 * During maintenance operations (optimize_index, compact_index), this log
 * captures all insert/remove operations so they can be replayed to the
 * optimized index clone before swapping.
 *
 * This enables non-blocking maintenance: queries continue on the active index
 * while a clone is being optimized in the background.
 *
 * Key design decisions:
 * - Uses std::vector to preserve operation order (critical for correctness)
 * - Has a MAX_ENTRIES limit to prevent unbounded growth
 * - Thread-safe with its own mutex
 */
struct WriteLog {
    /**
     * @brief Type of write operation
     */
    enum class Operation {
        Insert,
        Remove
    };

    /**
     * @brief A single log entry
     */
    struct Entry {
        Operation op;                                      ///< Operation type
        std::uint64_t id;                                  ///< Vector ID
        std::vector<float> vector;                         ///< Vector data (empty for Remove)
        std::chrono::steady_clock::time_point timestamp;   ///< When operation occurred
    };

    /// Maximum log entries before maintenance should abort
    static constexpr std::size_t kMaxEntries = 100'000;

    /// Threshold to warn about high write load during maintenance
    static constexpr std::size_t kWarnThreshold = 50'000;

    /// Log entries (ordered chronologically)
    std::vector<Entry> entries;

    /// Mutex for thread-safe access
    mutable std::mutex mutex;

    /// Whether logging is enabled (during maintenance)
    std::atomic<bool> enabled{false};

    /**
     * @brief Log an insert operation
     * @param id Vector ID
     * @param vector Vector data
     * @return true if logged successfully, false if log is full
     */
    bool log_insert(std::uint64_t id, std::span<const float> vector) {
        std::lock_guard lock(mutex);
        if (entries.size() >= kMaxEntries) {
            return false;  // Log overflow - abort maintenance
        }
        entries.push_back({
            Operation::Insert,
            id,
            std::vector<float>(vector.begin(), vector.end()),
            std::chrono::steady_clock::now()
        });
        return true;
    }

    /**
     * @brief Log a remove operation
     * @param id Vector ID
     * @return true if logged successfully, false if log is full
     */
    bool log_remove(std::uint64_t id) {
        std::lock_guard lock(mutex);
        if (entries.size() >= kMaxEntries) {
            return false;  // Log overflow - abort maintenance
        }
        entries.push_back({
            Operation::Remove,
            id,
            {},
            std::chrono::steady_clock::now()
        });
        return true;
    }

    /**
     * @brief Replay all logged operations to a target index
     * @param target The index to replay operations to
     */
    void replay_to(HNSWIndex* target) {
        std::lock_guard lock(mutex);

        for (const auto& entry : entries) {
            switch (entry.op) {
                case Operation::Insert: {
                    auto result = target->add(entry.id, entry.vector);

                    // Handle case where ID already exists in clone
                    if (result == ErrorCode::InvalidState) {
                        target->remove(entry.id);
                        target->add(entry.id, entry.vector);
                    }
                    break;
                }
                case Operation::Remove:
                    // Ignore if doesn't exist
                    target->remove(entry.id);
                    break;
            }
        }
    }

    /**
     * @brief Clear all log entries
     */
    void clear() {
        std::lock_guard lock(mutex);
        entries.clear();
    }

    /**
     * @brief Get current number of log entries
     * @return Number of entries
     */
    std::size_t size() const {
        std::lock_guard lock(mutex);
        return entries.size();
    }

    /**
     * @brief Check if log is at warning threshold
     * @return true if size exceeds warning threshold
     */
    bool is_at_warning_threshold() const {
        return size() > kWarnThreshold;
    }
};

} // namespace lynx

#endif // LYNX_WRITE_LOG_H
