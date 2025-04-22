#pragma once
#include <string>
#include "database.h"

std::string read_history(PersistenceManager& db, const std::string& start_time, const std::string& end_time, size_t limit);
