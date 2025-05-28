# Chronological Layout API Reference

## Overview

This document provides comprehensive API reference for the hybrid chronological-force layout algorithm implementation. It covers all classes, methods, parameters, and integration points for developers working with the layout system.

## Core Classes

### ForceDirectedLayout

The main class implementing the hybrid chronological-force layout algorithm.

**Header:** [`graph_layout.h`](../graph_layout.h:35)  
**Implementation:** [`graph_layout.cpp`](../graph_layout.cpp:52)

#### Constructor

```cpp
ForceDirectedLayout(const LayoutParams& params = LayoutParams())
```

**Parameters:**
- `params`: Layout configuration parameters (optional, uses defaults if not provided)

**Example:**
```cpp
// Use default parameters
ForceDirectedLayout layout;

// Use custom parameters
ForceDirectedLayout::LayoutParams custom_params;
custom_params.temporal_strength = 0.2f;
ForceDirectedLayout layout(custom_params);
```

#### Public Methods

##### Initialize

```cpp
void Initialize(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center)
```

Initializes the layout system with a set of nodes and prepares for simulation.

**Parameters:**
- `nodes`: Vector of graph nodes to layout
- `canvas_center`: Center point of the layout canvas

**Behavior:**
- Sets up physics data for each node
- Applies chronological initialization if enabled
- Resets simulation state

**Example:**
```cpp
std::vector<GraphNode*> nodes = GetConversationNodes();
ImVec2 center(1000.0f, 750.0f);
layout.Initialize(nodes, center);
```

##### UpdateLayout

```cpp
bool UpdateLayout(const std::vector<GraphNode*>& nodes)
```

Performs one iteration of the force-directed simulation.

**Parameters:**
- `nodes`: Vector of nodes to update

**Returns:**
- `true` if simulation should continue
- `false` if converged or maximum iterations reached

**Example:**
```cpp
while (layout.IsRunning()) {
    bool continue_simulation = layout.UpdateLayout(nodes);
    if (!continue_simulation) break;
    
    // Render current state for animation
    RenderNodes(nodes);
}
```

##### ComputeLayout

```cpp
void ComputeLayout(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center)
```

Runs the complete layout algorithm until convergence.

**Parameters:**
- `nodes`: Vector of nodes to layout
- `canvas_center`: Center point of the layout canvas

**Behavior:**
- Calls `Initialize()` internally
- Runs simulation until convergence or max iterations
- Blocks until completion

**Example:**
```cpp
layout.ComputeLayout(nodes, ImVec2(1000.0f, 750.0f));
// Layout is complete when this returns
```

##### IsRunning

```cpp
bool IsRunning() const
```

**Returns:** `true` if simulation is currently active, `false` otherwise

##### GetParams / SetParams

```cpp
const LayoutParams& GetParams() const
void SetParams(const LayoutParams& params)
```

Get or set the current layout parameters.

**Example:**
```cpp
// Get current parameters
auto current_params = layout.GetParams();

// Modify and set new parameters
current_params.temporal_strength = 0.3f;
layout.SetParams(current_params);
```

##### SetAnimationSpeed

```cpp
void SetAnimationSpeed(float speed_multiplier)
```

Adjusts the animation speed by modifying the time step.

**Parameters:**
- `speed_multiplier`: Multiplier for animation speed (1.0 = normal, 2.0 = double speed, 0.5 = half speed)

##### ResetPhysicsState

```cpp
void ResetPhysicsState()
```

Resets all physics data (velocities, forces) for all nodes.

##### PinNode

```cpp
void PinNode(GraphNode* node, bool pinned = true)
```

Pins or unpins a node at its current position.

**Parameters:**
- `node`: Node to pin/unpin
- `pinned`: `true` to pin, `false` to unpin

### LayoutParams

Configuration structure for the layout algorithm.

**Header:** [`graph_layout.h`](../graph_layout.h:37)

#### Force Parameters

```cpp
struct LayoutParams {
    float spring_strength;      // Default: 0.05f
    float repulsion_strength;   // Default: 50000.0f
    float damping_factor;       // Default: 0.85f
    float min_distance;         // Default: 200.0f
    float ideal_edge_length;    // Default: 400.0f
    // ...
};
```

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `spring_strength` | `float` | 0.05f | 0.01-0.2 | Attractive force between connected nodes |
| `repulsion_strength` | `float` | 50000.0f | 10000-100000 | Repulsive force between all nodes |
| `damping_factor` | `float` | 0.85f | 0.5-0.95 | Velocity damping coefficient |
| `min_distance` | `float` | 200.0f | 100-500 | Minimum distance between nodes |
| `ideal_edge_length` | `float` | 400.0f | 200-800 | Target distance for connected nodes |

#### Chronological Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `temporal_strength` | `float` | 0.1f | 0.0-1.0 | Strength of chronological ordering forces |
| `vertical_bias` | `float` | 0.3f | 0.0-1.0 | Bias toward vertical chronological arrangement |
| `chronological_spacing` | `float` | 150.0f | 50-500 | Vertical spacing between chronologically adjacent messages |
| `use_chronological_init` | `bool` | true | - | Enable chronological initialization |

#### Simulation Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `time_step` | `float` | 0.008f | 0.001-0.02 | Simulation time step |
| `max_iterations` | `int` | 500 | 50-2000 | Maximum simulation iterations |
| `convergence_threshold` | `float` | 0.1f | 0.01-1.0 | Energy threshold for convergence |
| `canvas_bounds` | `ImVec2` | (2000,1500) | - | Layout area boundaries |

#### Constructor

```cpp
LayoutParams()
```

Creates parameters with default values optimized for typical conversation layouts.

**Example:**
```cpp
// Use defaults
ForceDirectedLayout::LayoutParams params;

// Customize for specific use case
params.temporal_strength = 0.2f;
params.vertical_bias = 0.5f;
params.max_iterations = 300;
```

### GraphNode

Represents a message node in the conversation graph.

**Header:** [`gui_interface/graph_types.h`](../gui_interface/graph_types.h:11)

#### Core Data Members

```cpp
struct GraphNode {
    int graph_node_id;              // Unique ID for this graph node
    int message_id;                 // Original message ID
    HistoryMessage message_data;    // Message content and metadata
    std::string label;              // Node display label
    // ...
};
```

#### Visual Properties

```cpp
ImVec2 position;                // Current position (updated by layout)
ImVec2 size;                    // Node size for rendering
```

#### State Flags

```cpp
bool is_expanded;               // Children visibility state
bool is_selected;               // Selection state
bool content_needs_refresh;     // Refresh flag
```

#### Relational Structure

```cpp
GraphNode* parent;                       // Parent node pointer
std::vector<GraphNode*> children;        // Child node pointers
std::vector<GraphNode*> alternative_paths; // Alternative branches
```

#### Layout Properties

```cpp
int depth;                       // Depth in conversation tree
ImU32 color;                    // Node color
```

#### Constructor

```cpp
GraphNode(int g_node_id, const HistoryMessage& msg_data)
```

**Parameters:**
- `g_node_id`: Unique graph node identifier
- `msg_data`: Message data to associate with this node

## Utility Functions

### ApplyForceDirectedLayout

```cpp
void ApplyForceDirectedLayout(
    const std::vector<GraphNode*>& nodes, 
    const ImVec2& canvas_center,
    const ForceDirectedLayout::LayoutParams& params = ForceDirectedLayout::LayoutParams()
)
```

Convenience function for one-shot layout computation.

**Parameters:**
- `nodes`: Nodes to layout
- `canvas_center`: Canvas center point
- `params`: Layout parameters (optional)

**Example:**
```cpp
// Simple one-shot layout
ApplyForceDirectedLayout(nodes, ImVec2(1000, 750));

// With custom parameters
ForceDirectedLayout::LayoutParams params;
params.temporal_strength = 0.2f;
ApplyForceDirectedLayout(nodes, ImVec2(1000, 750), params);
```

### Legacy Function

#### CalculateNodePositionsRecursive

```cpp
void CalculateNodePositionsRecursive(
    GraphNode* node, 
    ImVec2 current_pos,
    float x_spacing, 
    float y_spacing, 
    int depth, 
    std::map<int, float>& level_x_offset, 
    const ImVec2& canvas_start_pos
)
```

Legacy recursive layout function (deprecated, use `ForceDirectedLayout` instead).

## Integration APIs

### Graph Manager Integration

The layout system integrates with [`GraphManager`](../graph_manager.h) through these interfaces:

#### Layout Triggering

```cpp
// In GraphManager
void TriggerLayout() {
    if (layout_algorithm_) {
        layout_algorithm_->ComputeLayout(GetAllNodes(), GetCanvasCenter());
    }
}
```

#### Animation Support

```cpp
// Incremental layout updates for animation
bool UpdateLayoutAnimation() {
    if (layout_algorithm_ && layout_algorithm_->IsRunning()) {
        return layout_algorithm_->UpdateLayout(GetAllNodes());
    }
    return false;
}
```

### Renderer Integration

Integration with [`GraphRenderer`](../graph_renderer.h) for visual updates:

#### Position Updates

```cpp
// Renderer reads positions from nodes
void RenderNode(const GraphNode* node) {
    ImVec2 screen_pos = WorldToScreen(node->position);
    // Render node at screen_pos
}
```

#### Animation Feedback

```cpp
// Visual feedback during layout computation
void RenderLayoutProgress(float progress) {
    // Show progress indicator
}
```

## Error Handling

### Common Error Conditions

#### Invalid Parameters

```cpp
// Parameter validation example
bool ValidateLayoutParams(const ForceDirectedLayout::LayoutParams& params) {
    if (params.spring_strength < 0.0f || params.spring_strength > 1.0f) {
        return false; // Invalid spring strength
    }
    if (params.max_iterations < 1) {
        return false; // Invalid iteration count
    }
    return true;
}
```

#### Null Node Handling

The layout system gracefully handles null nodes in input vectors:

```cpp
// Safe to include null pointers
std::vector<GraphNode*> nodes = {node1, nullptr, node2, node3};
layout.ComputeLayout(nodes, center); // Null nodes are skipped
```

#### Memory Management

Nodes are not owned by the layout system:

```cpp
// Layout does not delete nodes
{
    ForceDirectedLayout layout;
    layout.ComputeLayout(nodes, center);
} // layout destructor does not affect nodes
```

## Performance Considerations

### Computational Complexity

- **Time per iteration**: O(n²) due to all-pairs repulsion calculation
- **Memory usage**: O(n) for physics data storage
- **Typical convergence**: 100-500 iterations

### Optimization Strategies

#### Early Termination

```cpp
// Monitor convergence for early stopping
ForceDirectedLayout::LayoutParams params;
params.convergence_threshold = 0.2f; // Stop earlier
params.max_iterations = 300;         // Limit computation time
```

#### Adaptive Parameters

```cpp
// Adjust parameters based on node count
ForceDirectedLayout::LayoutParams GetOptimalParams(size_t node_count) {
    ForceDirectedLayout::LayoutParams params;
    
    if (node_count > 50) {
        params.max_iterations = 200;        // Faster for large graphs
        params.convergence_threshold = 0.3f; // Less precise
    }
    
    return params;
}
```

### Memory Usage

#### Physics Data Storage

Each node requires approximately 32 bytes of physics data:

```cpp
struct NodePhysics {
    ImVec2 velocity;  // 8 bytes
    ImVec2 force;     // 8 bytes
    bool is_fixed;    // 1 byte + padding
};
```

#### Estimation Formula

```cpp
size_t EstimateMemoryUsage(size_t node_count) {
    return node_count * sizeof(NodePhysics) + 
           node_count * sizeof(GraphNode*); // Vector storage
}
```

## Thread Safety

### Thread Safety Guarantees

- **Read Operations**: Safe from multiple threads
- **Write Operations**: Not thread-safe, require external synchronization
- **Layout Computation**: Should run on single thread

### Safe Usage Patterns

#### Background Layout Computation

```cpp
// Safe pattern for background layout
std::thread layout_thread([&]() {
    std::lock_guard<std::mutex> lock(layout_mutex);
    layout.ComputeLayout(nodes, center);
});
```

#### UI Thread Integration

```cpp
// Update UI from layout thread
void OnLayoutComplete() {
    // Post to UI thread
    PostToUIThread([this]() {
        RefreshNodePositions();
    });
}
```

## Testing and Validation

### Unit Testing

The [`test_chronological_layout.cpp`](../test_chronological_layout.cpp) provides comprehensive testing:

```cpp
// Run all validation tests
ChronologicalLayoutTester tester;
tester.RunAllTests();
```

### Test Scenarios

1. **Linear Conversation**: [`TestLinearConversation()`](../test_chronological_layout.cpp:70)
2. **Branched Discussion**: [`TestBranchedConversation()`](../test_chronological_layout.cpp:137)
3. **Mixed Input Order**: [`TestMixedChronologicalInput()`](../test_chronological_layout.cpp:196)
4. **Time Gap Handling**: [`TestVaryingTimeGaps()`](../test_chronological_layout.cpp:253)
5. **Parameter Validation**: [`TestParameterValidation()`](../test_chronological_layout.cpp:307)
6. **Performance Testing**: [`TestPerformance()`](../test_chronological_layout.cpp:354)

### Validation Functions

```cpp
// Verify chronological ordering
bool VerifyChronologicalOrder(const std::vector<GraphNode*>& nodes);

// Verify conversation structure preservation
bool VerifyConversationStructure(const std::vector<GraphNode*>& nodes);

// Verify depth-based positioning
bool VerifyDepthPositioning(const std::vector<GraphNode*>& nodes);
```

## Migration Guide

### From Legacy Layout

If migrating from the legacy recursive layout:

```cpp
// Old approach
std::map<int, float> level_x_offset;
CalculateNodePositionsRecursive(root_node, start_pos, x_spacing, y_spacing, 0, level_x_offset, canvas_start);

// New approach
ForceDirectedLayout layout;
layout.ComputeLayout(all_nodes, canvas_center);
```

### Parameter Migration

Legacy parameters can be mapped to new system:

```cpp
// Convert legacy spacing to new parameters
ForceDirectedLayout::LayoutParams ConvertLegacyParams(float x_spacing, float y_spacing) {
    ForceDirectedLayout::LayoutParams params;
    params.ideal_edge_length = x_spacing * 2.0f;
    params.chronological_spacing = y_spacing;
    return params;
}
```

## Examples

### Basic Usage Example

```cpp
#include "graph_layout.h"
#include "gui_interface/graph_types.h"

void LayoutConversation(std::vector<GraphNode*>& nodes) {
    // Create layout with default parameters
    ForceDirectedLayout layout;
    
    // Compute layout
    ImVec2 canvas_center(1000.0f, 750.0f);
    layout.ComputeLayout(nodes, canvas_center);
    
    // Nodes now have updated positions
}
```

### Animated Layout Example

```cpp
void AnimatedLayout(std::vector<GraphNode*>& nodes) {
    ForceDirectedLayout layout;
    ImVec2 canvas_center(1000.0f, 750.0f);
    
    // Initialize layout
    layout.Initialize(nodes, canvas_center);
    
    // Animate layout computation
    while (layout.IsRunning()) {
        bool should_continue = layout.UpdateLayout(nodes);
        
        // Render current state
        RenderNodes(nodes);
        
        if (!should_continue) break;
        
        // Control frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}
```

### Custom Parameters Example

```cpp
void CustomLayoutForLargeConversation(std::vector<GraphNode*>& nodes) {
    // Create optimized parameters for large conversations
    ForceDirectedLayout::LayoutParams params;
    params.temporal_strength = 0.15f;      // Moderate chronological force
    params.vertical_bias = 0.4f;           // Prefer vertical arrangement
    params.max_iterations = 300;           // Limit computation time
    params.convergence_threshold = 0.2f;   // Accept less precise convergence
    
    ForceDirectedLayout layout(params);
    layout.ComputeLayout(nodes, ImVec2(1500.0f, 1000.0f));
}
```

## Troubleshooting

### Common Issues and Solutions

#### Layout Not Converging

```cpp
// Increase damping and reduce time step
params.damping_factor = 0.9f;
params.time_step = 0.005f;
params.max_iterations = 1000;
```

#### Poor Chronological Order

```cpp
// Strengthen temporal forces
params.temporal_strength = 0.3f;
params.vertical_bias = 0.6f;
params.use_chronological_init = true;
```

#### Performance Issues

```cpp
// Optimize for speed
params.max_iterations = 200;
params.convergence_threshold = 0.3f;
params.time_step = 0.01f;
```

### Debug Information

Enable debug output for troubleshooting:

```cpp
// Add debug output to layout computation
void DebugLayout(const std::vector<GraphNode*>& nodes) {
    ForceDirectedLayout layout;
    layout.Initialize(nodes, ImVec2(1000, 750));
    
    int iteration = 0;
    while (layout.IsRunning()) {
        bool should_continue = layout.UpdateLayout(nodes);
        
        if (iteration % 50 == 0) {
            float energy = CalculateTotalEnergy(nodes);
            std::cout << "Iteration " << iteration << ", Energy: " << energy << std::endl;
        }
        
        if (!should_continue) break;
        iteration++;
    }
    
    std::cout << "Layout converged after " << iteration << " iterations" << std::endl;
}
```

This API reference provides comprehensive documentation for developers integrating and extending the chronological layout system. For usage examples and parameter tuning guidance, refer to the [User Guide](chronological-layout-user-guide.md).