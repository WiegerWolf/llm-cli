#pragma once
#include <vector>
#include <string>
#include <memory>

struct Message {
    std::string role;
    std::string content;
    int id = 0;
};

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();
    
    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content);
    std::vector<Message> getContextHistory(size_t max_pairs = 10);
    void trimDatabaseHistory(size_t keep_pairs = 20);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
