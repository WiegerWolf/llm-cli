#include "spatial_hash.h"
#include "gui_interface/graph_types.h"
#include <cmath>
#include <algorithm>

namespace detail {
    float Distance(const ImVec2& a, const ImVec2& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    ImVec2 Normalize(const ImVec2& vec) {
        float length = std::sqrt(vec.x * vec.x + vec.y * vec.y);
        if (length > 0.001f) {
            return ImVec2(vec.x / length, vec.y / length);
        }
        return ImVec2(0.0f, 0.0f);
    }
}

SpatialHash::SpatialHash(float cell_size) : cell_size_(std::max(1.0f, cell_size)) {}

void SpatialHash::Insert(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    nodes_ = &nodes;
    buckets_.clear();
    buckets_.reserve(nodes.size() * 2);

    for (size_t idx = 0; idx < nodes.size(); ++idx) {
        const auto& n = nodes[idx];
        if (!n) continue;

        int32_t cx = static_cast<int32_t>(std::floor(n->position.x / cell_size_));
        int32_t cy = static_cast<int32_t>(std::floor(n->position.y / cell_size_));
        buckets_[detail::PackCell(cx, cy)].push_back(static_cast<int>(idx));
    }
}

std::vector<int> SpatialHash::Query(const ImVec2& position, float radius) {
    if (!nodes_) return {};

    std::vector<int> result;
    int32_t center_cx = static_cast<int32_t>(std::floor(position.x / cell_size_));
    int32_t center_cy = static_cast<int32_t>(std::floor(position.y / cell_size_));
    
    int search_radius = static_cast<int>(std::ceil(radius / cell_size_));

    for (int dx = -search_radius; dx <= search_radius; ++dx) {
        for (int dy = -search_radius; dy <= search_radius; ++dy) {
            uint64_t key = detail::PackCell(center_cx + dx, center_cy + dy);
            auto bucket_it = buckets_.find(key);
            if (bucket_it != buckets_.end()) {
                result.insert(result.end(), bucket_it->second.begin(), bucket_it->second.end());
            }
        }
    }
    return result;
}