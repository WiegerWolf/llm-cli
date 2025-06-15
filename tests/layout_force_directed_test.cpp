#include "gtest/gtest.h"
#include "force_directed_layout.h"
#include <gui/views/graph_types.h> // For GraphNode
#include <memory>
#include <vector>

TEST(ForceDirectedLayoutTest, ConvergesAndAvoidsOverlap) {
    HistoryMessage msg1, msg2, msg3;
    msg1.message_id = 1;
    msg2.message_id = 2;
    msg3.message_id = 3;

    auto node1 = std::make_shared<GraphNode>(1, msg1);
    auto node2 = std::make_shared<GraphNode>(2, msg2);
    auto node3 = std::make_shared<GraphNode>(3, msg3);
    
    node1->position = ImVec2(5, 5);
    node2->position = ImVec2(6, 6); // Start them very close
    node3->position = ImVec2(5, 6);

    std::vector<std::shared_ptr<GraphNode>> nodes = {node1, node2, node3};
    node1->add_child(node2);
    node2->add_child(node3);

    ForceDirectedLayout::LayoutParams params;
    params.max_iterations = 500;
    params.repulsion_strength = 100.0f;
    params.min_distance = 20.0f; // Node size is 10, so min_distance > 10*sqrt(2) is needed

    ForceDirectedLayout layout(params);

    ImVec2 canvas_center(100, 100);
    layout.ComputeLayout(nodes, canvas_center);

    // Verify non-overlapping positions
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            float dist_sq = (nodes[i]->position.x - nodes[j]->position.x) * (nodes[i]->position.x - nodes[j]->position.x) +
                            (nodes[i]->position.y - nodes[j]->position.y) * (nodes[i]->position.y - nodes[j]->position.y);
            float min_dist_sq = params.min_distance * params.min_distance;
            EXPECT_GT(dist_sq, min_dist_sq * 0.81); // Allow for some tolerance
        }
    }
}