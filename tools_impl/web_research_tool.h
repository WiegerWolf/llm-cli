#pragma once
#include <string>
#include "database.h"
class ChatClient;
class UserInterface; // Forward declaration

std::string perform_web_research(PersistenceManager& db, ChatClient& client, UserInterface& ui, const std::string& topic);
