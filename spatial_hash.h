#ifndef SPATIAL_HASH_H
#define SPATIAL_HASH_H

#include "extern/imgui/imgui.h"
#include <unordered_map>
#include "gui_interface/graph_types.h"
#include <vector>
#include <memory>
#include <cstdint>

// Helper: pack 2D grid cell coordinates into 64-bit key for unordered_map buckets
constexpr uint64_t PackCell(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
           static_cast<uint32_t>(y);
}

float Distance(const ImVec2& a, const ImVec2& b);
ImVec2 Normalize(const ImVec2& vec);

class SpatialHash {
public:
    SpatialHash(float cell_size);
    void Insert(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    std::vector<int> Query(const ImVec2& position, float radius);
    
private:
    float cell_size_;
    std::unordered_map<uint64_t, std::vector<int>> buckets_;
    const std::vector<std::shared_ptr<GraphNode>>* nodes_ = nullptr;
};

#endif // SPATIAL_HASH_H