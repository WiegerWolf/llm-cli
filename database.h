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
    void saveToolMessage(const std::string& content); // Added for tool results
    std::vector<Message> getContextHistory(size_t max_pairs = 10);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
