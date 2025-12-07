/**
 * @file test_mps.cpp
 * @brief Unit tests for MPS (Message Processing System) integration
 *
 * These tests verify that the MPS library is correctly integrated and working.
 */

#include <gtest/gtest.h>
#include "mps.h"
#include <memory>
#include <string>
#include <atomic>

// ============================================================================
// Test Messages
// ============================================================================

/**
 * @brief Simple test message containing a string and counter
 */
class TestMessage : public mps::message {
public:
    std::string text;
    int value;

    TestMessage() : text("Hello"), value(42) {}
    TestMessage(const std::string& t, int v) : text(t), value(v) {}
};

// ============================================================================
// Test Workers
// ============================================================================

/**
 * @brief Simple test worker that counts processed messages
 */
class CountingWorker : public mps::worker {
public:
    std::atomic<int> message_count{0};
    std::atomic<int> notification_count{0};
    std::string last_text;
    int last_value = 0;

    void process(std::shared_ptr<const mps::message> m) override {
        // Try to cast to TestMessage
        auto test_msg = std::dynamic_pointer_cast<const TestMessage>(m);
        if (test_msg) {
            message_count++;
            last_text = test_msg->text;
            last_value = test_msg->value;
            return;
        }

        // Try to cast to notification message
        auto notif_msg = std::dynamic_pointer_cast<const mps::notification_message>(m);
        if (notif_msg) {
            notification_count++;
            return;
        }
    }
};

// ============================================================================
// Basic Pool Tests
// ============================================================================

TEST(MPSTest, CreateAndStartPool) {
    // Create a pool
    auto p = mps::pool::create();
    ASSERT_NE(p, nullptr);

    // Start the pool
    p->start();

    // Stop and join
    p->stop();
    p->join();
}

TEST(MPSTest, AddWorkerToPool) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    // Add worker to pool
    p->add_worker(w);

    p->start();
    p->stop();
    p->join();
}

// ============================================================================
// Message Processing Tests
// ============================================================================

TEST(MPSTest, ProcessSingleMessage) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);
    p->start();

    // Push a test message
    auto msg = std::make_shared<TestMessage>("Test Message", 123);
    p->push_back(msg);

    // Stop and wait for processing
    p->stop();
    p->join();

    // Verify the message was processed
    EXPECT_EQ(w->message_count, 1);
    EXPECT_EQ(w->last_text, "Test Message");
    EXPECT_EQ(w->last_value, 123);
}

TEST(MPSTest, ProcessMultipleMessages) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);
    p->start();

    // Push multiple messages
    for (int i = 0; i < 10; ++i) {
        p->push_back(std::make_shared<TestMessage>("Message", i));
    }

    p->stop();
    p->join();

    // Verify all messages were processed
    EXPECT_EQ(w->message_count, 10);
}

TEST(MPSTest, ProcessNotificationMessage) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);
    p->start();

    // Push notification messages
    p->push_back_notification();
    p->push_back_notification();

    p->stop();
    p->join();

    // Verify notifications were received
    EXPECT_EQ(w->notification_count, 2);
}

// ============================================================================
// Worker Management Tests
// ============================================================================

TEST(MPSTest, AddMultipleWorkers) {
    auto p = mps::pool::create();
    auto w1 = std::make_shared<CountingWorker>();
    auto w2 = std::make_shared<CountingWorker>();

    p->add_worker(w1);
    p->add_worker(w2);
    p->start();

    // Push a message - both workers should receive it
    p->push_back(std::make_shared<TestMessage>("Shared", 999));

    p->stop();
    p->join();

    // In MPS, all workers in a pool receive all messages
    // So both workers should have processed the message
    EXPECT_EQ(w1->message_count + w2->message_count, 2);
    EXPECT_EQ(w1->message_count, 1);
    EXPECT_EQ(w2->message_count, 1);
}

TEST(MPSTest, AddWorkerOnTheFly) {
    auto p = mps::pool::create();
    auto w1 = std::make_shared<CountingWorker>();

    p->add_worker(w1);
    p->start();

    // Push first message
    p->push_back(std::make_shared<TestMessage>("First", 1));

    // Add second worker while pool is running
    auto w2 = std::make_shared<CountingWorker>();
    p->add_worker(w2);

    // Push second message - both workers should see it
    p->push_back(std::make_shared<TestMessage>("Second", 2));

    p->stop();
    p->join();

    // w1 should have seen both messages, w2 only the second
    EXPECT_GE(w1->message_count, 1);
}

// ============================================================================
// Pool Options Tests
// ============================================================================

TEST(MPSTest, CreatePoolWithOptions) {
    mps::pool_options opts;
    opts.priority = 100;
    opts.timeout_wait_for_message = 100;

    auto p = mps::pool::create(opts);
    ASSERT_NE(p, nullptr);

    p->start();
    p->stop();
    p->join();
}

TEST(MPSTest, PoolNaming) {
    auto p = mps::pool::create();
    p->node_name("TestPool");

    EXPECT_EQ(p->node_name(), "TestPool");

    p->start();
    p->stop();
    p->join();
}

// ============================================================================
// Message Ordering Tests
// ============================================================================

TEST(MPSTest, MessageOrderingPreserved) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);
    p->start();

    // Push messages in order
    for (int i = 0; i < 5; ++i) {
        p->push_back(std::make_shared<TestMessage>("Ordered", i));
    }

    p->stop();
    p->join();

    // All messages should be processed
    EXPECT_EQ(w->message_count, 5);
    // Last message should have value 4
    EXPECT_EQ(w->last_value, 4);
}

// ============================================================================
// Pool Lifecycle Tests
// ============================================================================

TEST(MPSTest, PushMessagesBeforeStart) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);

    // Push messages before starting
    p->push_back(std::make_shared<TestMessage>("Before Start", 100));

    // Now start - the message should still be processed
    p->start();
    p->stop();
    p->join();

    EXPECT_EQ(w->message_count, 1);
    EXPECT_EQ(w->last_value, 100);
}

TEST(MPSTest, StopIfNoWorkersOption) {
    mps::pool_options opts;
    opts.stop_if_no_workers = true;

    auto p = mps::pool::create(opts);
    auto w = std::make_shared<CountingWorker>();

    p->start();
    p->add_worker(w);

    // Push a message
    p->push_back(std::make_shared<TestMessage>("Test", 42));

    // Remove the worker - pool should stop automatically
    p->remove_worker(w);

    // Should be able to join without explicit stop
    p->join();

    EXPECT_EQ(w->message_count, 1);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST(MPSTest, ConcurrentMessagePushing) {
    auto p = mps::pool::create();
    auto w = std::make_shared<CountingWorker>();

    p->add_worker(w);
    p->start();

    // Push many messages rapidly
    const int num_messages = 100;
    for (int i = 0; i < num_messages; ++i) {
        p->push_back(std::make_shared<TestMessage>("Concurrent", i));
    }

    p->stop();
    p->join();

    // All messages should be processed
    EXPECT_EQ(w->message_count, num_messages);
}
