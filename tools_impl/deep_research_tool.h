#pragma once
#include <string>
#include "database.h"
class ChatClient;

std::string perform_deep_research(PersistenceManager& db, ChatClient& client, const std::string& goal);
