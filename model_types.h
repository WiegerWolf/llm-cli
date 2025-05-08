#ifndef MODEL_TYPES_H
#define MODEL_TYPES_H

#include <string>
#include <vector> // Included for potential future use, not strictly necessary for ModelData alone

// Defines the structure for holding AI model attributes.
// This structure is used to represent models fetched from the API
// and stored in the local database.
struct ModelData {
    std::string id;          // e.g., "openai/gpt-4"
    std::string name;        // e.g., "GPT-4"
    // Add other relevant fields based on API response, e.g.:
    // int context_length;
    // double input_cost_per_mtok;
    // double output_cost_per_mtok;
    // std::string architecture;
    // bool supports_tools;

    // Default constructor
    ModelData() = default;

    // Parameterized constructor (optional, but can be useful)
    ModelData(std::string id, std::string name)
        : id(std::move(id)), name(std::move(name)) {}
};

#endif // MODEL_TYPES_H