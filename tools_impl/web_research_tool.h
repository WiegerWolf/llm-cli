#pragma once
#include <string>
#include "database.h"
class ChatClient;

std::string perform_web_research(PersistenceManager& db, ChatClient& client, const std::string& topic);
