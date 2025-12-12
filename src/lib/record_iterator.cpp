/**
 * @file record_iterator.cpp
 * @brief Implementation of record iterator classes
 *
 * @copyright MIT License
 */

#include "../include/lynx/lynx.h"
#include "record_iterator_impl.h"

namespace lynx {

// ============================================================================
// RecordIterator Implementation
// ============================================================================

RecordIterator::RecordIterator(std::shared_ptr<RecordIteratorImpl> impl)
    : impl_(std::move(impl)) {}

RecordIterator::RecordIterator(const RecordIterator& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}

RecordIterator& RecordIterator::operator=(const RecordIterator& other) {
    if (this != &other) {
        impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    }
    return *this;
}

RecordIterator::~RecordIterator() = default;

RecordIterator::reference RecordIterator::operator*() const {
    return impl_->dereference();
}

RecordIterator::pointer RecordIterator::operator->() const {
    return &impl_->dereference();
}

RecordIterator& RecordIterator::operator++() {
    impl_->increment();
    return *this;
}

RecordIterator RecordIterator::operator++(int) {
    RecordIterator tmp(*this);
    impl_->increment();
    return tmp;
}

bool RecordIterator::operator==(const RecordIterator& other) const {
    if (!impl_ && !other.impl_) return true;
    if (!impl_ || !other.impl_) return false;
    return impl_->equals(*other.impl_);
}

bool RecordIterator::operator!=(const RecordIterator& other) const {
    return !(*this == other);
}

// ============================================================================
// RecordRange Implementation
// ============================================================================

RecordRange::RecordRange(RecordIterator begin_iter, RecordIterator end_iter)
    : begin_(std::move(begin_iter)), end_(std::move(end_iter)) {}

RecordIterator RecordRange::begin() const {
    return begin_;
}

RecordIterator RecordRange::end() const {
    return end_;
}

} // namespace lynx
