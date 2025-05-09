#ifndef MODEL_TYPES_H
#define MODEL_TYPES_H

#include <string>
#include <vector> // Included for potential future use, not strictly necessary for ModelData alone

// Defines the structure for holding AI model attributes.
// This structure is used to represent models fetched from the API
// and stored in the local database.
struct ModelData {
    std::string id;                             // TEXT, PRIMARY KEY - Model ID from API
    std::string name;                           // TEXT - Human-readable model name
    std::string description;                    // TEXT - Model description
    int context_length = 0;                 // INTEGER - Model context length
    std::string pricing_prompt;                 // TEXT - Cost per prompt token
    std::string pricing_completion;             // TEXT - Cost per completion token
    std::string architecture_input_modalities;  // TEXT - JSON array of strings
    std::string architecture_output_modalities; // TEXT - JSON array of strings
    std::string architecture_tokenizer;         // TEXT
    bool top_provider_is_moderated = false; // INTEGER - Boolean (0 or 1)
    std::string per_request_limits;             // TEXT - JSON object as string
    std::string supported_parameters;           // TEXT - JSON array of strings
    long long created_at_api = 0;             // INTEGER - Timestamp from API `created` field
    std::string last_updated_db;                // TIMESTAMP - When this record was last updated

    // Default constructor
    ModelData() = default;

    // Parameterized constructor (optional, but can be useful)
    // Add more parameters as needed or rely on member-wise initialization
    ModelData(std::string id, std::string name)
        : id(std::move(id)), name(std::move(name)) {}
};

#endif // MODEL_TYPES_H