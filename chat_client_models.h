#pragma once

#include <atomic>
#include <future>
#include <string>
#include <vector>

// Forward declarations to avoid including full headers, reducing compile-time dependencies.
class UserInterface;
class Database;
struct ModelData;

namespace chat {

/// @brief Initializes the model management system.
///
/// This function is the main entry point for model loading. It behaves synchronously
/// from the caller's perspective but uses an asynchronous task internally to fetch,
/// parse, and cache models from the API or local database. It updates the UI
/// with loading status and handles fallbacks to default models if errors occur.
///
/// @param ui Reference to the UserInterface for displaying status and errors.
/// @param db Reference to the Database for caching and retrieving models.
/// @param active_model_id Reference to the string holding the current active model ID. This will be updated.
/// @param model_load_future Reference to a std::future to hold the asynchronous task.
/// @param models_loading Reference to an atomic flag indicating if models are being loaded.
void initialize_model_manager(
    UserInterface& ui,
    Database& db,
    std::string& active_model_id,
    std::future<void>& model_load_future,
    std::atomic<bool>& models_loading
);

/// @brief Gets the OpenRouter API key from compile-time definitions or environment variables.
/// @return The API key as a string.
/// @throws std::runtime_error if the key is not found.
std::string get_openrouter_api_key();

} // namespace chat