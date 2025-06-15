#include "gtest/gtest.h"
#include "message_store.h"
#include "sqlite_connection.h"
#include <memory>

class DBMessageStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_db_conn = std::make_unique<app::db::SQLiteConnection>();
        m_db_conn->exec("DROP TABLE IF EXISTS messages;");
        m_db_conn->exec("CREATE TABLE messages (id INTEGER PRIMARY KEY, role TEXT, content TEXT, timestamp TEXT, model_id TEXT);");
        m_store = std::make_unique<app::db::MessageStore>(*m_db_conn);
    }

    void TearDown() override {
        m_store.reset();
        m_db_conn.reset();
    }

    std::unique_ptr<app::db::SQLiteConnection> m_db_conn;
    std::unique_ptr<app::db::MessageStore> m_store;
};

TEST_F(DBMessageStoreTest, InsertAndFetch) {
    m_store->saveUserMessage("Hello, world!");
    auto messages = m_store->getContextHistory(1);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].role, "user");
    EXPECT_EQ(messages[0].content, "Hello, world!");
}

TEST_F(DBMessageStoreTest, List) {
    m_store->saveUserMessage("Message 1");
    m_store->saveAssistantMessage("Message 2", "gpt-4");
    m_store->saveUserMessage("Message 3");
    
    auto messages = m_store->getContextHistory(5);
    ASSERT_EQ(messages.size(), 3);
    EXPECT_EQ(messages[0].content, "Message 1");
    EXPECT_EQ(messages[1].content, "Message 2");
    EXPECT_EQ(messages[2].content, "Message 3");
}

TEST_F(DBMessageStoreTest, NoOrphans) {
    // Valid JSON tool messages
    const char* tool_msg1 = R"({"tool_call_id":"call_123","name":"test_tool","content":"payload"})";
    const char* tool_msg2 = R"({"tool_call_id":"call_456","name":"test_tool_2","content":"payload2"})";

    // Insert orphan tool message and clean up
    m_store->saveToolMessage(tool_msg1);
    m_store->cleanupOrphanedToolMessages();
    auto ctx_after_cleanup = m_store->getContextHistory(2);
    // Expect only the default system message to remain.
    ASSERT_EQ(ctx_after_cleanup.size(), 1);
    EXPECT_EQ(ctx_after_cleanup[0].role, "system");

    // Insert an assistant message that calls a tool, followed by the tool's response.
    const char* assistant_json = R"({"tool_calls":[{"id":"call_456","type":"function","function":{"name":"test_tool_2","arguments":"{}"}}]})";
    m_store->saveAssistantMessage(assistant_json, "model");
    m_store->saveToolMessage(tool_msg2);

    // Cleanup shouldn't remove the valid tool message since it's preceded by its call
    m_store->cleanupOrphanedToolMessages();
    auto messages = m_store->getContextHistory(5);

    // Expect assistant + tool = 2 messages
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0].role, "assistant");
    EXPECT_EQ(messages[1].role, "tool");
}