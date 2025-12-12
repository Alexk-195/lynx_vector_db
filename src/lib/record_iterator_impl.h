/**
 * @file record_iterator_impl.h
 * @brief Internal implementation interface for record iterators
 *
 * This file defines the RecordIteratorImpl base class that database
 * implementations must derive from to provide iteration support.
 *
 * @copyright MIT License
 */

#ifndef LYNX_RECORD_ITERATOR_IMPL_H
#define LYNX_RECORD_ITERATOR_IMPL_H

#include "../include/lynx/lynx.h"
#include <memory>
#include <unordered_map>
#include <shared_mutex>

namespace lynx {

/**
 * @brief Base class for iterator implementation (pImpl pattern).
 *
 * Each database implementation provides its own derived class
 * that wraps the underlying container iterator and manages locking.
 */
class RecordIteratorImpl {
public:
    virtual ~RecordIteratorImpl() = default;

    /**
     * @brief Dereference the iterator to get current record
     * @return Reference to current key-value pair
     */
    virtual const std::pair<const std::uint64_t, VectorRecord>& dereference() const = 0;

    /**
     * @brief Increment the iterator to next element
     */
    virtual void increment() = 0;

    /**
     * @brief Check equality with another iterator
     * @param other Iterator to compare with
     * @return true if iterators point to same position
     */
    virtual bool equals(const RecordIteratorImpl& other) const = 0;

    /**
     * @brief Clone this iterator
     * @return Shared pointer to new iterator copy
     */
    virtual std::shared_ptr<RecordIteratorImpl> clone() const = 0;
};

// ============================================================================
// Concrete Implementation Templates
// ============================================================================

/**
 * @brief Simple iterator implementation for non-thread-safe databases
 *
 * This template wraps an unordered_map iterator without any locking.
 */
template<typename MapType>
class SimpleIteratorImpl : public RecordIteratorImpl {
public:
    using Iterator = typename MapType::const_iterator;

    explicit SimpleIteratorImpl(Iterator it) : it_(it) {}

    const std::pair<const std::uint64_t, VectorRecord>& dereference() const override {
        return *it_;
    }

    void increment() override {
        ++it_;
    }

    bool equals(const RecordIteratorImpl& other) const override {
        auto* other_ptr = dynamic_cast<const SimpleIteratorImpl*>(&other);
        if (!other_ptr) return false;
        return it_ == other_ptr->it_;
    }

    std::shared_ptr<RecordIteratorImpl> clone() const override {
        return std::make_shared<SimpleIteratorImpl>(it_);
    }

private:
    Iterator it_;
};

/**
 * @brief Thread-safe iterator implementation with shared lock
 *
 * This template wraps an unordered_map iterator and holds a shared_lock
 * for the lifetime of the iterator. The lock is shared among all copies
 * of the iterator through a shared_ptr.
 */
template<typename MapType, typename MutexType>
class LockedIteratorImpl : public RecordIteratorImpl {
public:
    using Iterator = typename MapType::const_iterator;
    using LockType = std::shared_lock<MutexType>;

    /**
     * @brief Construct locked iterator
     * @param it Underlying map iterator
     * @param lock Shared lock (will be shared among copies)
     */
    LockedIteratorImpl(Iterator it, std::shared_ptr<LockType> lock)
        : it_(it), lock_(std::move(lock)) {}

    const std::pair<const std::uint64_t, VectorRecord>& dereference() const override {
        return *it_;
    }

    void increment() override {
        ++it_;
    }

    bool equals(const RecordIteratorImpl& other) const override {
        auto* other_ptr = dynamic_cast<const LockedIteratorImpl*>(&other);
        if (!other_ptr) return false;
        return it_ == other_ptr->it_;
    }

    std::shared_ptr<RecordIteratorImpl> clone() const override {
        return std::make_shared<LockedIteratorImpl>(it_, lock_);
    }

private:
    Iterator it_;
    std::shared_ptr<LockType> lock_;  ///< Shared lock kept alive across copies
};

} // namespace lynx

#endif // LYNX_RECORD_ITERATOR_IMPL_H
