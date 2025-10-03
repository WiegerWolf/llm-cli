#include "model_manager.h"
#include "config.h"
#include "curl_utils.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <memory>

// Helper function to get OpenRouter API key
static std::string get_openrouter_api_key() {
    constexpr const char* compiled_key = OPENROUTER_API_KEY;
    if (compiled_key[0] != '\0' && std::string(compiled_key) != "@OPENROUTER_API_KEY@") {
        return std::string(compiled_key);
    }
    const char* env_key = std::getenv("OPENROUTER_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    throw std::runtime_error("OPENROUTER_API_KEY not set at compile time or in environment");
}

ModelManager::ModelManager(UserInterface& ui_ref, PersistenceManager& db_ref)
    : ui(ui_ref), db(db_ref), active_model_id(DEFAULT_MODEL_ID) {
    // Constructor initializes with default model ID as fallback
}

ModelManager::~ModelManager() {
    if (model_load_future.valid()) {
        try {
            model_load_future.wait();
        } catch (const std::exception& e) {
            // Errors already handled in initialize(), just wait for completion
        } catch (...) {
            // Silent cleanup
        }
    }
}

void ModelManager::initialize() {
    ui.setLoadingModelsState(true);
    
    model_load_future = std::async(std::launch::async, &ModelManager::loadModelsAsync, this);
    
    try {
        model_load_future.get();
    } catch (const std::exception& e) {
        ui.displayError("Critical error during model initialization: " + std::string(e.what()));
        ui.displayStatus("Fell back to default model: " + this->active_model_id);
    }
    
    ui.setLoadingModelsState(false);
    ui.displayStatus("Model manager initialized. Active model: " + this->active_model_id);
    
    try {
        ui.updateModelsList(db.getAllModels());
    } catch (const std::exception& e) {
        ui.displayError("Failed to update UI with model list after initialization: " + std::string(e.what()));
    }
}

void ModelManager::loadModelsAsync() {
    models_loading = true;
    
    std::string previously_selected_model_id;
    bool is_first_launch = false;
    
    try {
        previously_selected_model_id = db.loadSetting("selected_model_id").value_or("");
        if (previously_selected_model_id.empty()) {
            try {
                if (db.getAllModels().empty()) {
                    is_first_launch = true;
                }
            } catch (const std::exception& db_e) {
                ui.displayError("Minor: Could not check cache for first launch determination: " + std::string(db_e.what()));
            }
        }
    } catch (const std::exception& e) {
        ui.displayError("Minor: Could not load previously selected model ID: " + std::string(e.what()));
    }
    
    try {
        ui.displayStatus("Attempting to fetch models from API...");
        std::string api_response_str = fetchModelsFromAPI();
        std::vector<ModelData> fetched_models = parseModelsFromAPIResponse(api_response_str);
        
        if (fetched_models.empty()) {
            ui.displayError("API returned no models. Will attempt to load from cache.");
            throw std::runtime_error("No models returned from API");
        }
        
        cacheModelsToDB(fetched_models);
        ui.displayStatus("Successfully fetched and cached " + std::to_string(fetched_models.size()) + " models from API.");
        selectActiveModel(fetched_models, "from API", previously_selected_model_id);
        
    } catch (const std::exception& api_or_parse_error) {
        std::string api_error_msg = "API Error: " + std::string(api_or_parse_error.what());
        ui.displayError(api_error_msg + ". Attempting to load from cache...");
        
        try {
            std::vector<ModelData> cached_models = db.getAllModels();
            if (cached_models.empty()) {
                this->active_model_id = DEFAULT_MODEL_ID;
                if (is_first_launch) {
                    ui.displayError("API unavailable and cache empty on first launch. Using default model: " + std::string(DEFAULT_MODEL_ID));
                } else {
                    ui.displayError("API unavailable and cache is empty. Using default model: " + std::string(DEFAULT_MODEL_ID));
                }
                try { db.saveSetting("selected_model_id", this->active_model_id); } catch (...) {}
            } else {
                ui.displayStatus("Successfully loaded " + std::to_string(cached_models.size()) + " models from cache (API was unavailable).");
                selectActiveModel(cached_models, "from cache", previously_selected_model_id);
            }
        } catch (const std::exception& db_error) {
            this->active_model_id = DEFAULT_MODEL_ID;
            ui.displayError("Failed to load models from cache: " + std::string(db_error.what()));
            if (is_first_launch) {
                ui.displayError("API and cache also failed on first launch. Using default model: " + std::string(DEFAULT_MODEL_ID));
            } else {
                ui.displayError("API and cache also failed. Using default model: " + std::string(DEFAULT_MODEL_ID));
            }
            try { db.saveSetting("selected_model_id", this->active_model_id); } catch (...) {}
        }
    }
    
    models_loading = false;
}

std::string ModelManager::fetchModelsFromAPI() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for fetching models.");
    }
    auto curl_guard = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>{curl, curl_easy_cleanup};
    
    std::string api_key = get_openrouter_api_key();
    
    struct curl_slist* headers = nullptr;
    auto headers_guard = std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)>{nullptr, curl_slist_free_all};
    
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com");
    headers = curl_slist_append(headers, "X-Title: LLM-cli");
    headers_guard.reset(headers);
    
    std::string response_buffer;
    const char* api_url = OPENROUTER_API_URL_MODELS;
    
    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error("API request to fetch models failed: " + std::string(curl_easy_strerror(res)));
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw std::runtime_error("API request to fetch models returned HTTP status " + std::to_string(http_code) + ". Response: " + response_buffer);
    }
    
    return response_buffer;
}

std::vector<ModelData> ModelManager::parseModelsFromAPIResponse(const std::string& api_response) {
    std::vector<ModelData> parsed_models;
    if (api_response.empty()) {
        throw std::runtime_error("API response string is empty, cannot parse models.");
    }
    
    try {
        nlohmann::json j = nlohmann::json::parse(api_response);
        
        if (j.contains("data") && j["data"].is_array()) {
            for (const auto& model_obj : j["data"]) {
                ModelData model_item;
                bool supports_tool_calling = false;
                
                model_item.id = model_obj.value("id", "");
                model_item.name = model_obj.value("name", "");
                if (model_item.name.empty()) {
                    model_item.name = model_item.id;
                }
                
                model_item.description = model_obj.value("description", "");
                model_item.context_length = model_obj.value("context_length", 0);
                model_item.created_at_api = model_obj.value("created", 0LL);
                
                // Parse pricing
                if (model_obj.contains("pricing") && model_obj["pricing"].is_object()) {
                    const auto& pricing_obj = model_obj["pricing"];
                    if (pricing_obj.contains("prompt")) {
                        if (pricing_obj["prompt"].is_string()) {
                            model_item.pricing_prompt = pricing_obj.value("prompt", "");
                        } else if (pricing_obj["prompt"].is_number()) {
                            model_item.pricing_prompt = std::to_string(pricing_obj["prompt"].get<double>());
                        }
                    }
                    if (pricing_obj.contains("completion")) {
                        if (pricing_obj["completion"].is_string()) {
                            model_item.pricing_completion = pricing_obj.value("completion", "");
                        } else if (pricing_obj["completion"].is_number()) {
                            model_item.pricing_completion = std::to_string(pricing_obj["completion"].get<double>());
                        }
                    }
                }
                
                // Parse architecture
                if (model_obj.contains("architecture") && model_obj["architecture"].is_object()) {
                    const auto& arch_obj = model_obj["architecture"];
                    model_item.architecture_tokenizer = arch_obj.value("tokenizer", "");
                    
                    if (arch_obj.contains("input_modalities") && arch_obj["input_modalities"].is_array()) {
                        model_item.architecture_input_modalities = arch_obj["input_modalities"].dump();
                    } else {
                        model_item.architecture_input_modalities = "[]";
                    }
                    
                    if (arch_obj.contains("output_modalities") && arch_obj["output_modalities"].is_array()) {
                        model_item.architecture_output_modalities = arch_obj["output_modalities"].dump();
                    } else {
                        model_item.architecture_output_modalities = "[]";
                    }
                }
                
                // Parse top_provider
                if (model_obj.contains("top_provider") && model_obj["top_provider"].is_object()) {
                    const auto& provider_obj = model_obj["top_provider"];
                    model_item.top_provider_is_moderated = provider_obj.value("is_moderated", false);
                }
                
                // Parse per_request_limits
                if (model_obj.contains("per_request_limits") && model_obj["per_request_limits"].is_object()) {
                    model_item.per_request_limits = model_obj["per_request_limits"].dump();
                } else {
                    model_item.per_request_limits = "{}";
                }
                
                // Parse supported_parameters and check for tool support
                if (model_obj.contains("supported_parameters") && model_obj["supported_parameters"].is_array()) {
                    const auto& sup_params = model_obj["supported_parameters"];
                    model_item.supported_parameters = sup_params.dump();
                    for (const auto& param : sup_params) {
                        if (param.is_string() && param.get<std::string>() == "tools") {
                            supports_tool_calling = true;
                            break;
                        }
                    }
                } else {
                    model_item.supported_parameters = "[]";
                }
                
                // Only add models that support tool calling
                if (!model_item.id.empty() && supports_tool_calling) {
                    parsed_models.push_back(model_item);
                }
            }
        } else {
            throw std::runtime_error("Failed to parse models: 'data' field not found or not an array. Response: " + api_response.substr(0, 500));
        }
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse models API response JSON: " + std::string(e.what()) + ". Response snippet: " + api_response.substr(0, 500));
    } catch (const std::exception& e) {
        throw std::runtime_error("An unexpected error occurred during model parsing: " + std::string(e.what()));
    }
    
    return parsed_models;
}

void ModelManager::cacheModelsToDB(const std::vector<ModelData>& models) {
    if (models.empty()) {
        return;
    }
    try {
        db.replaceModelsInDB(models);
    } catch (const std::exception& e) {
        throw;
    }
}

void ModelManager::selectActiveModel(const std::vector<ModelData>& available_models, 
                                     const std::string& context_msg,
                                     const std::string& previously_selected_model_id) {
    bool model_set = false;
    
    // Try previously selected model first
    if (!previously_selected_model_id.empty()) {
        for (const auto& model : available_models) {
            if (model.id == previously_selected_model_id) {
                this->active_model_id = previously_selected_model_id;
                model_set = true;
                ui.displayStatus("Using previously selected model (" + context_msg + "): " + this->active_model_id);
                break;
            }
        }
    }
    
    // Try default model
    if (!model_set) {
        for (const auto& model : available_models) {
            if (model.id == DEFAULT_MODEL_ID) {
                this->active_model_id = DEFAULT_MODEL_ID;
                model_set = true;
                ui.displayStatus("Using default model ID (" + context_msg + "): " + this->active_model_id);
                break;
            }
        }
    }
    
    // Use first available model
    if (!model_set && !available_models.empty()) {
        this->active_model_id = available_models[0].id;
        ui.displayStatus("Using first available model (" + context_msg + "): " + this->active_model_id);
    } else if (!model_set) {
        this->active_model_id = DEFAULT_MODEL_ID;
        ui.displayError("No suitable model found in available list (" + context_msg + "). Using compile-time default: " + this->active_model_id);
    }
    
    // Persist selection
    try {
        db.saveSetting("selected_model_id", this->active_model_id);
    } catch (const std::exception& e) {
        ui.displayError("Warning: Could not persist selected model ID: " + std::string(e.what()));
    }
}

void ModelManager::setActiveModel(const std::string& model_id) {
    // Validate model exists
    auto model = db.getModelById(model_id);
    if (!model.has_value()) {
        ui.displayError("Model '" + model_id + "' not found.");
        return; // Don't change active_model_id if validation fails
    }
    
    // Set the active model
    this->active_model_id = model_id;
    
    // Persist the selection
    try {
        db.saveSetting("selected_model_id", model_id);
    } catch (const std::exception& e) {
        ui.displayError("Warning: Could not persist model selection: " + std::string(e.what()));
    }
    
    // Display success
    ui.displayStatus("Active model set to: " + model->name + " (" + model_id + ")");
}