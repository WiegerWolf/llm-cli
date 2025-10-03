# Database Module Migration Guide

## Overview

This guide documents the refactored database module architecture for the LLM CLI project. The monolithic `database.cpp` (725 lines) has been restructured into a modular design following SOLID principles.

## What Changed?

### Before (Monolithic Design)
```
database.cpp (725 lines)
├── Connection management
├── Schema initialization
├── Message operations
├── Model operations
└── Settings operations
```

### After (Modular Design)
```
database/
├── database_core.cpp (250 lines)      - Connection & transactions
├── message_repository.cpp (300 lines) - Message operations
└── model_repository.cpp (250 lines)   - Model operations

database.cpp (158 lines)               - Facade pattern
```

## Architecture Overview

### Component Diagram

```
┌─────────────────────────────────────┐
│    PersistenceManager (Facade)      │  ← Public API (unchanged)
│    - No breaking changes            │
│    - All existing code works        │
└──────────────┬──────────────────────┘
               │
       ┌───────┴───────┐
       │               │
       ▼               ▼
┌─────────────┐  ┌─────────────┐
│  Messages   │  │   Models    │
│ Repository  │  │ Repository  │
└──────┬──────┘  └──────┬──────┘
       │                │
       └────────┬───────┘
                │
                ▼
        ┌───────────────┐
        │ DatabaseCore  │
        │ - Connection  │
        │ - Transactions│
        │ - Schema      │
        └───────────────┘
```

## Module Responsibilities

### 1. DatabaseCore (`database/database_core.{h,cpp}`)

**Purpose:** Foundation layer for SQLite operations

**Responsibilities:**
- SQLite connection lifecycle (open/close)
- Database path resolution (cross-platform)
- Schema initialization
- Migration management
- Transaction management (BEGIN/COMMIT/ROLLBACK)
- RAII wrappers for prepared statements

**Key Classes:**
```cpp
namespace database {
    struct SQLiteStmtDeleter;        // RAII for sqlite3_stmt
    using unique_stmt_ptr = ...;
    
    class DatabaseCore {
        void beginTransaction();
        void commitTransaction();
        void rollbackTransaction();
        unique_stmt_ptr prepareStatement(const std::string& sql);
        void exec(const std::string& sql);
        sqlite3* getConnection();
    };
}
```

**Usage Example:**
```cpp
database::DatabaseCore core;
core.beginTransaction();
try {
    auto stmt = core.prepareStatement("INSERT INTO ...");
    // ... bind and execute ...
    core.commitTransaction();
} catch (...) {
    core.rollbackTransaction();
    throw;
}
```

### 2. MessageRepository (`database/message_repository.{h,cpp}`)

**Purpose:** All message-related database operations

**Responsibilities:**
- Message insertion (user/assistant/tool)
- Context history retrieval for API calls
- Time-range message queries
- Tool message validation
- Orphaned tool message cleanup

**Key Methods:**
```cpp
namespace database {
    class MessageRepository {
        void insertUserMessage(const std::string& content);
        void insertAssistantMessage(const std::string& content, 
                                     const std::string& model_id);
        void insertToolMessage(const std::string& content);
        
        std::vector<Message> getContextHistory(size_t max_pairs = 10);
        std::vector<Message> getHistoryRange(const std::string& start,
                                              const std::string& end,
                                              size_t limit = 50);
        
        void cleanupOrphanedToolMessages();
    };
}
```

**Usage Example:**
```cpp
database::DatabaseCore core;
database::MessageRepository messages(core);

messages.insertUserMessage("Hello, AI!");
messages.insertAssistantMessage("Hello! How can I help?", "gpt-4");

auto history = messages.getContextHistory(10);
```

### 3. ModelRepository (`database/model_repository.{h,cpp}`)

**Purpose:** Model metadata storage and retrieval

**Responsibilities:**
- Model CRUD operations
- Bulk model replacement (atomic)
- Model queries by ID
- Model name lookup for UI

**Key Methods:**
```cpp
namespace database {
    class ModelRepository {
        void insertOrUpdateModel(const ModelData& model);
        void clearAllModels();
        void replaceModels(const std::vector<ModelData>& models);
        
        std::vector<ModelData> getAllModels();
        std::optional<ModelData> getModelById(const std::string& id);
        std::optional<std::string> getModelNameById(const std::string& id);
    };
}
```

**Usage Example:**
```cpp
database::DatabaseCore core;
database::ModelRepository models(core);

ModelData model;
model.id = "gpt-4";
model.name = "GPT-4";
models.insertOrUpdateModel(model);

auto all = models.getAllModels();
auto gpt4 = models.getModelById("gpt-4");
```

### 4. PersistenceManager (Facade - `database.{h,cpp}`)

**Purpose:** Maintain backwards compatibility and provide unified interface

**Key Points:**
- **Public API unchanged** - All existing code continues to work
- Delegates to appropriate repositories
- Manages repository lifecycle
- Coordinates transactions across repositories

**Implementation:**
```cpp
struct PersistenceManager::Impl {
    database::DatabaseCore core;
    database::MessageRepository messages;
    database::ModelRepository models;
    
    Impl() : core(), messages(core), models(core) {}
};

void PersistenceManager::saveUserMessage(const std::string& content) {
    impl->messages.insertUserMessage(content);  // Delegates
}
```

## Working with the New Architecture

### For New Features

#### Adding a New Message Type

**Where to make changes:** `database/message_repository.{h,cpp}`

1. Add method to MessageRepository:
```cpp
// In message_repository.h
void insertSystemMessage(const std::string& content);

// In message_repository.cpp
void MessageRepository::insertSystemMessage(const std::string& content) {
    Message msg;
    msg.role = "system";
    msg.content = content;
    msg.model_id = std::nullopt;
    insertMessage(msg);
}
```

2. Add facade method in PersistenceManager:
```cpp
// In database.h
void saveSystemMessage(const std::string& content);

// In database.cpp
void PersistenceManager::saveSystemMessage(const std::string& content) {
    impl->messages.insertSystemMessage(content);
}
```

#### Adding Model Caching

**Where to make changes:** `database/model_repository.{h,cpp}`

```cpp
// In model_repository.h
class ModelRepository {
private:
    std::unordered_map<std::string, ModelData> cache_;
    
public:
    std::optional<ModelData> getModelById(const std::string& id) {
        // Check cache first
        if (auto it = cache_.find(id); it != cache_.end()) {
            return it->second;
        }
        // Query database
        auto model = queryModelFromDB(id);
        if (model) {
            cache_[id] = *model;
        }
        return model;
    }
};
```

#### Adding a New Repository

1. Create `database/new_repository.{h,cpp}`
2. Implement following the pattern of existing repositories
3. Add to `PersistenceManager::Impl`:
```cpp
struct PersistenceManager::Impl {
    database::DatabaseCore core;
    database::MessageRepository messages;
    database::ModelRepository models;
    database::NewRepository newRepo;  // Add here
    
    Impl() 
        : core()
        , messages(core)
        , models(core)
        , newRepo(core)  // Initialize with core
    {}
};
```
4. Update `CMakeLists.txt` to include new files

### For Bug Fixes

#### Message-Related Bug
**File:** `database/message_repository.cpp`  
**Scope:** ~300 lines, focused on messages only

#### Model-Related Bug
**File:** `database/model_repository.cpp`  
**Scope:** ~250 lines, focused on models only

#### Connection/Transaction Bug
**File:** `database/database_core.cpp`  
**Scope:** ~250 lines, core infrastructure only

### For Maintenance

#### Adding Database Migration

**File:** `database/database_core.cpp`

```cpp
void DatabaseCore::runMigrations() {
    // Existing migration...
    
    // Add new migration
    bool new_column_exists = checkColumnExists("table_name", "new_column");
    if (!new_column_exists) {
        exec("ALTER TABLE table_name ADD COLUMN new_column TEXT;");
    }
}
```

## Testing Strategy

### Unit Testing (Future Enhancement)

The modular design enables independent unit testing:

```cpp
// Test MessageRepository in isolation
TEST(MessageRepository, InsertUserMessage) {
    MockDatabaseCore mock_core;
    database::MessageRepository repo(mock_core);
    
    EXPECT_CALL(mock_core, prepareStatement(_))
        .WillOnce(Return(mock_stmt));
    
    repo.insertUserMessage("test");
}
```

### Integration Testing

```cpp
// Test through facade (current approach)
PersistenceManager db;
db.saveUserMessage("test");
auto history = db.getContextHistory(1);
ASSERT_EQ(history.size(), 2);  // System + user message
```

## Best Practices

### 1. Always Use RAII

✅ **Good:**
```cpp
auto stmt = core.prepareStatement(sql);
// stmt automatically finalized
```

❌ **Bad:**
```cpp
sqlite3_stmt* stmt;
sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
// Must manually finalize - error prone
```

### 2. Repository Methods Should Be Atomic

✅ **Good:**
```cpp
void ModelRepository::replaceModels(const std::vector<ModelData>& models) {
    core_.beginTransaction();
    try {
        clearAllModels();
        for (const auto& m : models) insertOrUpdateModel(m);
        core_.commitTransaction();
    } catch (...) {
        core_.rollbackTransaction();
        throw;
    }
}
```

### 3. Error Handling

All repository methods throw `std::runtime_error` on failure:

```cpp
try {
    messages.insertUserMessage("content");
} catch (const std::runtime_error& e) {
    std::cerr << "DB error: " << e.what() << std::endl;
}
```

### 4. Null Handling

Use `std::optional` for nullable fields:

```cpp
Message msg;
msg.model_id = std::nullopt;  // NULL in database

if (msg.model_id.has_value()) {
    std::cout << "Model: " << msg.model_id.value() << std::endl;
}
```

### 5. SQL Injection Prevention

Always use prepared statements with parameter binding:

```cpp
// ✅ Good - parameterized
auto stmt = core.prepareStatement("SELECT * FROM messages WHERE id = ?");
sqlite3_bind_int(stmt.get(), 1, message_id);

// ❌ Bad - string concatenation (vulnerable to SQL injection)
std::string sql = "SELECT * FROM messages WHERE id = " + std::to_string(id);
```

## Performance Considerations

### Prepared Statement Reuse

For repeated queries, prepare once:

```cpp
class MessageRepository {
    unique_stmt_ptr insert_stmt_;  // Reusable statement
    
    void insertMessage(const Message& msg) {
        if (!insert_stmt_) {
            insert_stmt_ = core_.prepareStatement(
                "INSERT INTO messages (role, content, model_id) VALUES (?, ?, ?)"
            );
        }
        // Bind and execute...
    }
};
```

### Transaction Batching

Batch multiple operations in a single transaction:

```cpp
void bulkInsertMessages(const std::vector<Message>& messages) {
    core_.beginTransaction();
    try {
        for (const auto& msg : messages) {
            insertMessage(msg);
        }
        core_.commitTransaction();
    } catch (...) {
        core_.rollbackTransaction();
        throw;
    }
}
```

## Migration Checklist

When working with the refactored code:

- [ ] Understand which repository handles your use case
- [ ] Check if facade method exists for backwards compatibility
- [ ] Use RAII wrappers (unique_stmt_ptr) for statements
- [ ] Wrap multi-step operations in transactions
- [ ] Handle errors appropriately (exceptions)
- [ ] Use prepared statements with parameter binding
- [ ] Return `std::optional` for nullable results
- [ ] Update `CMakeLists.txt` if adding new files
- [ ] Test compilation after changes
- [ ] Consider adding unit tests for new functionality

## Backwards Compatibility Guarantee

### All Existing Code Works Unchanged

✅ **These continue to work exactly as before:**

```cpp
PersistenceManager db;
db.saveUserMessage("Hello");
db.saveAssistantMessage("Hi!", "gpt-4");
db.saveToolMessage(tool_json);
auto history = db.getContextHistory(10);
auto models = db.getAllModels();
db.beginTransaction();
db.commitTransaction();
db.saveSetting("key", "value");
```

### No Header Changes Required

All files including `database.h` continue to work:
- `chat_client.h`
- `command_handler.h`
- `model_manager.h`
- `tool_executor.h`
- `api_client.h`
- All tools in `tools_impl/`

## File Structure Reference

```
llm-cli/
├── database.h                          (Public API - 60 lines)
├── database.cpp                        (Facade - 158 lines)
├── database/
│   ├── database_core.h                 (Core interface - 80 lines)
│   ├── database_core.cpp               (Core impl - 250 lines)
│   ├── message_repository.h            (Messages interface - 60 lines)
│   ├── message_repository.cpp          (Messages impl - 300 lines)
│   ├── model_repository.h              (Models interface - 70 lines)
│   └── model_repository.cpp            (Models impl - 250 lines)
├── CMakeLists.txt                      (Updated to include database/)
└── REFACTORING_PLAN.md                 (Detailed architecture plan)
```

## Common Tasks Quick Reference

| Task | File(s) | Complexity |
|------|---------|------------|
| Add message type | `message_repository.{h,cpp}` | Low |
| Fix message query | `message_repository.cpp` | Low |
| Add model field | `model_repository.cpp` + `model_types.h` | Medium |
| Fix model caching | `model_repository.cpp` | Low |
| Add database migration | `database_core.cpp::runMigrations()` | Medium |
| Fix connection issue | `database_core.cpp` | Medium |
| Add new repository | Create new files + update `database.cpp` | High |
| Update build | `CMakeLists.txt` | Low |

## Troubleshooting

### Compilation Errors

**Error:** `undefined reference to database::DatabaseCore`  
**Solution:** Ensure `database/database_core.cpp` is in CMakeLists.txt

**Error:** `class PersistenceManager::Impl has no member 'messages'`  
**Solution:** Ensure Impl struct in database.cpp defines all repositories

### Runtime Errors

**Error:** `Database connection failed`  
**Solution:** Check home directory permissions and .llm-cli folder

**Error:** `SQL error`  
**Solution:** Check SQL syntax in repository method, enable SQLite error messages

## Questions?

Refer to:
1. [`REFACTORING_PLAN.md`](REFACTORING_PLAN.md) - Detailed architecture documentation
2. Repository header files for interface documentation
3. Existing repository implementations for patterns and examples

---

**Document Version:** 1.0  
**Last Updated:** 2025-10-03  
**Author:** Database Refactoring Team