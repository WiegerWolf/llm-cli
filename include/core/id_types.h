#pragma once
#include <cstdint>

using NodeIdType = std::int64_t;

// Sentinel value representing an invalid / uninitialized node id
constexpr NodeIdType kInvalidNodeId = static_cast<NodeIdType>(-1);