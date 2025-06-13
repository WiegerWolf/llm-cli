#include <iostream>
#include <vector>
#include "id_types.h" // NodeIdType
#include <chrono>     // std::chrono utilities
#include <cstdlib>    // std::llabs
#include <chrono>
#include <random>
#include <cassert>
#include <algorithm>
#include <iomanip>
#include "graph_layout.h"
#include "gui_interface/graph_types.h"
#include "gui_interface/gui_interface.h"

// Test utility functions
class ChronologicalLayoutTester {
private:
    std::vector<std::shared_ptr<GraphNode>> test_nodes_;
    ForceDirectedLayout layout_;
    
public:
    ChronologicalLayoutTester() : layout_(CreateTestLayoutParams()) {}
    
    ~ChronologicalLayoutTester() {
        // Clean up test nodes
    }
    
    // Create test layout parameters optimized for testing
    static ForceDirectedLayout::LayoutParams CreateTestLayoutParams() {
        ForceDirectedLayout::LayoutParams params;
        params.spring_strength = 0.05f;
        params.repulsion_strength = 30000.0f;
        params.damping_factor = 0.85f;
        params.min_distance = 150.0f;
        params.ideal_edge_length = 300.0f;
        params.time_step = 0.01f;
        params.max_iterations = 200; // Reduced for testing
        params.convergence_threshold = 0.5f;
        params.canvas_bounds = ImVec2(1500.0f, 1200.0f);
        params.temporal_strength = 0.15f;
        params.vertical_bias = 0.4f;
        params.chronological_spacing = 120.0f;
        params.use_chronological_init = true;
        return params;
    }
    
    // Create a test message with specified timestamp and content
    HistoryMessage CreateTestMessage(int id, long long timestamp, const std::string& content, int parent_id = -1) {
        HistoryMessage msg;
        msg.message_id = static_cast<NodeIdType>(id);
        msg.type = MessageType::USER_INPUT;
        msg.content = content;
        msg.timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(
                            std::chrono::milliseconds(timestamp));
        msg.parent_id = static_cast<NodeIdType>(parent_id);
        return msg;
    }
    
    // Create a test node with specified parameters
    std::shared_ptr<GraphNode> CreateTestNode(int id, long long timestamp, const std::string& content, int depth = 0, int parent_id = -1) {
        HistoryMessage msg = CreateTestMessage(id, timestamp, content, parent_id);
        auto node = std::make_shared<GraphNode>(static_cast<NodeIdType>(id), msg);
        node->depth = depth;
        node->size = ImVec2(200.0f, 80.0f); // Standard node size
        node->label = "Node " + std::to_string(id);
        test_nodes_.push_back(node);
        return node;
    }
    
    // Test Scenario 1: Simple linear conversation
    void TestLinearConversation() {
        std::cout << "\n=== Test 1: Linear Conversation ===" << std::endl;
        
        // Clear previous test data
        ClearTestData();
        
        // Create a simple linear conversation with 5 messages
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        auto node1 = CreateTestNode(1, base_time, "Hello, how are you?", 0);
        auto node2 = CreateTestNode(2, base_time + 60000, "I'm doing well, thanks!", 1);
        auto node3 = CreateTestNode(3, base_time + 120000, "What are you working on?", 0);
        auto node4 = CreateTestNode(4, base_time + 180000, "I'm testing a layout algorithm", 1);
        auto node5 = CreateTestNode(5, base_time + 240000, "That sounds interesting!", 0);
        
        // Set up parent-child relationships
        node2->parent = node1;
        node1->add_child(node2);

        node3->parent = node2;
        node2->add_child(node3);

        node4->parent = node3;
        node3->add_child(node4);

        node5->parent = node4;
        node4->add_child(node5);
        
        std::vector<GraphNode*> nodes;
        for(const auto& n : test_nodes_) nodes.push_back(n.get());
        
        // Test chronological initialization
        std::cout << "Testing chronological initialization..." << std::endl;
        PrintInitialPositions(nodes);
        
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Positions after chronological initialization:" << std::endl;
        PrintNodePositions(nodes);
        
        // Verify chronological ordering
        bool chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order maintained: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        
        // Run force simulation
        std::cout << "\nRunning force simulation..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Force simulation completed in " << duration.count() << "ms" << std::endl;
        std::cout << "Final positions after force simulation:" << std::endl;
        PrintNodePositions(nodes);
        
        // Verify chronological order is still maintained
        chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order after simulation: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        
        // Verify conversation structure
        bool structure_maintained = VerifyConversationStructure(nodes);
        std::cout << "Conversation structure maintained: " << (structure_maintained ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test Scenario 2: Branched conversation
    void TestBranchedConversation() {
        std::cout << "\n=== Test 2: Branched Conversation ===" << std::endl;
        
        ClearTestData();
        
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create a branched conversation
        auto root = CreateTestNode(1, base_time, "What's your favorite programming language?", 0);
        auto reply1 = CreateTestNode(2, base_time + 30000, "I like Python for its simplicity", 1);
        auto reply2 = CreateTestNode(3, base_time + 45000, "C++ is powerful for performance", 1);
        auto reply3 = CreateTestNode(4, base_time + 60000, "JavaScript is versatile", 1);
        auto follow_up1 = CreateTestNode(5, base_time + 90000, "Python is great for data science", 2);
        auto follow_up2 = CreateTestNode(6, base_time + 120000, "C++ requires more careful memory management", 2);
        
        // Set up branched structure
        reply1->parent = root;
        reply2->parent = root;
        reply3->parent = root;
        root->add_child(reply1);
        root->add_child(reply2);
        root->add_child(reply3);

        follow_up1->parent = reply1;
        reply1->add_child(follow_up1);

        follow_up2->parent = reply2;
        reply2->add_child(follow_up2);
        
        std::vector<GraphNode*> nodes;
        for(const auto& n : test_nodes_) nodes.push_back(n.get());
        
        std::cout << "Testing branched conversation layout..." << std::endl;
        PrintInitialPositions(nodes);
        
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Positions after initialization:" << std::endl;
        PrintNodePositions(nodes);
        
        // Run simulation
        auto start_time = std::chrono::high_resolution_clock::now();
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Simulation completed in " << duration.count() << "ms" << std::endl;
        std::cout << "Final positions:" << std::endl;
        PrintNodePositions(nodes);
        
        // Verify results
        bool chronological_order = VerifyChronologicalOrder(nodes);
        bool structure_maintained = VerifyConversationStructure(nodes);
        bool depth_positioning = VerifyDepthPositioning(nodes);
        
        std::cout << "Chronological order: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        std::cout << "Conversation structure: " << (structure_maintained ? "PASS" : "FAIL") << std::endl;
        std::cout << "Depth positioning: " << (depth_positioning ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test Scenario 3: Mixed chronological order input
    void TestMixedChronologicalInput() {
        std::cout << "\n=== Test 3: Mixed Chronological Order Input ===" << std::endl;
        
        ClearTestData();
        
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create nodes with deliberately mixed timestamps
        auto node3 = CreateTestNode(3, base_time + 120000, "Third message", 0);
        auto node1 = CreateTestNode(1, base_time, "First message", 0);
        auto node5 = CreateTestNode(5, base_time + 240000, "Fifth message", 0);
        auto node2 = CreateTestNode(2, base_time + 60000, "Second message", 0);
        auto node4 = CreateTestNode(4, base_time + 180000, "Fourth message", 0);
        
        // Set up linear chain (but nodes created out of order)
        node2->parent = node1;
        node1->add_child(node2);

        node3->parent = node2;
        node2->add_child(node3);

        node4->parent = node3;
        node3->add_child(node4);

        node5->parent = node4;
        node4->add_child(node5);
        
        // Input nodes in mixed order to test sorting
        std::vector<GraphNode*> nodes;
        for(const auto& n : test_nodes_) nodes.push_back(n.get());
        
        std::cout << "Input nodes in mixed chronological order:" << std::endl;
        for (size_t i = 0; i < nodes.size(); ++i) {
            std::cout << "  Input[" << i << "]: Node " << nodes[i]->graph_node_id 
                      << " (timestamp: " << nodes[i]->message_data.timestamp << ")" << std::endl;
        }
        
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Positions after chronological sorting and initialization:" << std::endl;
        PrintNodePositions(nodes);
        
        // Verify that sorting worked correctly
        bool chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological sorting: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        
        // Run simulation
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Final positions after simulation:" << std::endl;
        PrintNodePositions(nodes);
        
        chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order maintained: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test Scenario 4: Varying time gaps
    void TestVaryingTimeGaps() {
        std::cout << "\n=== Test 4: Varying Time Gaps ===" << std::endl;
        
        ClearTestData();
        
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create messages with varying time gaps
        auto node1 = CreateTestNode(1, base_time, "Message 1", 0);
        auto node2 = CreateTestNode(2, base_time + 30000, "Message 2 (30s later)", 1);  // 30 seconds
        auto node3 = CreateTestNode(3, base_time + 60000, "Message 3 (30s later)", 0);  // 30 seconds
        auto node4 = CreateTestNode(4, base_time + 3660000, "Message 4 (1h later)", 1); // 1 hour gap
        auto node5 = CreateTestNode(5, base_time + 3720000, "Message 5 (1m later)", 0); // 1 minute
        
        // Set up relationships
        node2->parent = node1;
        node1->add_child(node2);

        node3->parent = node2;
        node2->add_child(node3);

        node4->parent = node3;
        node3->add_child(node4);

        node5->parent = node4;
        node4->add_child(node5);
        
        std::vector<GraphNode*> nodes;
        for(const auto& n : test_nodes_) nodes.push_back(n.get());
        
        std::cout << "Testing messages with varying time gaps:" << std::endl;
        for (GraphNode* node : nodes) {
            std::cout << "  Node " << node->graph_node_id << ": " << node->message_data.content << std::endl;
        }
        
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Positions after initialization:" << std::endl;
        PrintNodePositions(nodes);
        
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        std::cout << "Final positions:" << std::endl;
        PrintNodePositions(nodes);
        
        // Verify chronological order and spacing
        bool chronological_order = VerifyChronologicalOrder(nodes);
        bool appropriate_spacing = VerifyTimeGapSpacing(nodes);
        
        std::cout << "Chronological order: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        std::cout << "Appropriate time gap spacing: " << (appropriate_spacing ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test parameter validation
    void TestParameterValidation() {
        std::cout << "\n=== Test 5: Parameter Validation ===" << std::endl;
        
        // Test different parameter combinations
        ForceDirectedLayout::LayoutParams params1 = CreateTestLayoutParams();
        params1.use_chronological_init = false;
        
        ForceDirectedLayout::LayoutParams params2 = CreateTestLayoutParams();
        params2.temporal_strength = 0.0f;
        
        ForceDirectedLayout::LayoutParams params3 = CreateTestLayoutParams();
        params3.vertical_bias = 1.0f;
        params3.chronological_spacing = 200.0f;
        
        std::cout << "Testing parameter combinations:" << std::endl;
        std::cout << "  Params 1: chronological_init=false" << std::endl;
        std::cout << "  Params 2: temporal_strength=0.0" << std::endl;
        std::cout << "  Params 3: vertical_bias=1.0, spacing=200" << std::endl;
        
        // Create simple test case
        ClearTestData();
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        auto node1 = CreateTestNode(1, base_time, "Test 1", 0);
        auto node2 = CreateTestNode(2, base_time + 60000, "Test 2", 1);
        auto node3 = CreateTestNode(3, base_time + 120000, "Test 3", 0);
        
        node2->parent = node1;
        node1->add_child(node2);
        node3->parent = node2;
        node2->add_child(node3);
        
        std::vector<GraphNode*> nodes;
        for(const auto& n : test_nodes_) nodes.push_back(n.get());
        
        // Test each parameter set
        std::vector<ForceDirectedLayout::LayoutParams> param_sets = {params1, params2, params3};
        for (size_t i = 0; i < param_sets.size(); ++i) {
            ForceDirectedLayout test_layout(param_sets[i]);
            test_layout.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
            
            bool chronological_order = VerifyChronologicalOrder(nodes);
            std::cout << "  Params " << (i + 1) << " result: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        }
    }
    
    // Performance test
    void TestPerformance() {
        std::cout << "\n=== Test 6: Performance Test ===" << std::endl;
        
        ClearTestData();
        
        // Create a larger graph for performance testing
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::vector<std::shared_ptr<GraphNode>> nodes;
        const int num_nodes = 20;
        
        // Create nodes
        for (int i = 0; i < num_nodes; ++i) {
            auto node = CreateTestNode(i + 1, base_time + i * 30000,
                                           "Performance test message " + std::to_string(i + 1),
                                           i % 3); // Vary depth
            nodes.push_back(node);
        }
        
        // Create some parent-child relationships
        for (int i = 1; i < num_nodes; ++i) {
            if (i % 3 != 0) { // Not every node has a parent
                nodes[i]->parent = nodes[i - 1];
                nodes[i - 1]->add_child(nodes[i]);
            }
        }
        
        std::cout << "Performance test with " << num_nodes << " nodes..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<GraphNode*> raw_nodes;
        for(const auto& n : nodes) raw_nodes.push_back(n.get());
        layout_.ComputeLayout(raw_nodes, ImVec2(750.0f, 600.0f));
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Layout computation completed in " << duration.count() << "ms" << std::endl;
        std::cout << "Average time per node: " << (duration.count() / static_cast<double>(num_nodes)) << "ms" << std::endl;
        
        bool chronological_order = VerifyChronologicalOrder(raw_nodes);
        std::cout << "Chronological order maintained: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        
        // Performance benchmark
        if (duration.count() < 1000) { // Should complete within 1 second
            std::cout << "Performance: PASS (< 1000ms)" << std::endl;
        } else {
            std::cout << "Performance: FAIL (>= 1000ms)" << std::endl;
        }
    }
    
    // Run all tests
    void RunAllTests() {
        std::cout << "=== Chronological Layout Algorithm Test Suite ===" << std::endl;
        std::cout << "Testing hybrid chronological-force layout implementation" << std::endl;
        
        TestLinearConversation();
        TestBranchedConversation();
        TestMixedChronologicalInput();
        TestVaryingTimeGaps();
        TestParameterValidation();
        TestPerformance();
        
        std::cout << "\n=== Test Suite Complete ===" << std::endl;
    }

private:
    void ClearTestData() {
        test_nodes_.clear();
        layout_.ResetPhysicsState();
    }
    
    void PrintInitialPositions(const std::vector<GraphNode*>& nodes) {
        std::cout << "Initial positions (before layout):" << std::endl;
        for (GraphNode* node : nodes) {
            std::cout << "  Node " << node->graph_node_id << ": (" 
                      << std::fixed << std::setprecision(1) 
                      << node->position.x << ", " << node->position.y << ")" << std::endl;
        }
    }
    
    void PrintNodePositions(const std::vector<GraphNode*>& nodes) {
        for (GraphNode* node : nodes) {
            std::cout << "  Node " << node->graph_node_id 
                      << " (timestamp: " << node->message_data.timestamp << "): ("
                      << std::fixed << std::setprecision(1) 
                      << node->position.x << ", " << node->position.y 
                      << ") depth=" << node->depth << std::endl;
        }
    }
    
    bool VerifyChronologicalOrder(const std::vector<GraphNode*>& nodes) {
        // Sort nodes by timestamp
        std::vector<GraphNode*> sorted_nodes = nodes;
        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const GraphNode* a, const GraphNode* b) {
                return a->message_data.timestamp < b->message_data.timestamp;
            });
        
        // Check that Y positions are in chronological order (older messages HIGHER up, newer messages LOWER down)
        for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
            if (sorted_nodes[i]->position.y > sorted_nodes[i + 1]->position.y) {
                std::cout << "    Chronological order violation: Node "
                          << sorted_nodes[i]->graph_node_id << " (y=" << sorted_nodes[i]->position.y
                          << ") should be above Node " << sorted_nodes[i + 1]->graph_node_id
                          << " (y=" << sorted_nodes[i + 1]->position.y << ")" << std::endl;
                return false;
            }
        }
        return true;
    }
    
    bool VerifyConversationStructure(const std::vector<GraphNode*>& nodes) {
        for (GraphNode* node : nodes) {
            // Check parent-child relationships are reasonable
            if (auto parent = node->parent_raw()) {
                float distance = std::sqrt(
                    (node->position.x - parent->position.x) * (node->position.x - parent->position.x) +
                    (node->position.y - parent->position.y) * (node->position.y - parent->position.y)
                );

                // Parent and child should be reasonably close
                if (distance > 800.0f) {
                    std::cout << "    Structure violation: Node " << node->graph_node_id
                              << " too far from parent " << parent->graph_node_id
                              << " (distance: " << distance << ")" << std::endl;
                    return false;
                }
            }
        }
        return true;
    }
    
    bool VerifyDepthPositioning(const std::vector<GraphNode*>& nodes) {
        // Nodes with greater depth should generally be positioned more to the right
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                if (nodes[i]->depth < nodes[j]->depth) {
                    // Allow some tolerance for force-directed adjustments
                    if (nodes[i]->position.x > nodes[j]->position.x + 100.0f) {
                        std::cout << "    Depth positioning issue: Node " << nodes[i]->graph_node_id 
                                  << " (depth " << nodes[i]->depth << ", x=" << nodes[i]->position.x 
                                  << ") should be left of Node " << nodes[j]->graph_node_id 
                                  << " (depth " << nodes[j]->depth << ", x=" << nodes[j]->position.x << ")" << std::endl;
                        return false;
                    }
                }
            }
        }
        return true;
    }
    
    bool VerifyTimeGapSpacing(const std::vector<GraphNode*>& nodes) {
        // Sort by timestamp
        std::vector<GraphNode*> sorted_nodes = nodes;
        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const GraphNode* a, const GraphNode* b) {
                return a->message_data.timestamp < b->message_data.timestamp;
            });
        
        // Check that larger time gaps correspond to larger Y spacing
        for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
            auto gap1_duration = sorted_nodes[i + 1]->message_data.timestamp - sorted_nodes[i]->message_data.timestamp;
            long long time_gap1 = std::chrono::duration_cast<std::chrono::milliseconds>(gap1_duration).count();
            float y_gap1 = sorted_nodes[i + 1]->position.y - sorted_nodes[i]->position.y;
            
            if (i < sorted_nodes.size() - 2) {
                auto gap2_duration = sorted_nodes[i + 2]->message_data.timestamp - sorted_nodes[i + 1]->message_data.timestamp;
                long long time_gap2 = std::chrono::duration_cast<std::chrono::milliseconds>(gap2_duration).count();
                float y_gap2 = sorted_nodes[i + 2]->position.y - sorted_nodes[i + 1]->position.y;
                
                // If time gap is significantly larger, Y gap should also be larger (with tolerance)
                if (time_gap1 > time_gap2 * 10 && y_gap1 < y_gap2 * 0.8f) {
                    std::cout << "    Time gap spacing issue: Larger time gap (" << time_gap1 
                              << "ms) has smaller Y spacing (" << y_gap1 << ") than smaller time gap (" 
                              << time_gap2 << "ms, Y spacing: " << y_gap2 << ")" << std::endl;
                    return false;
                }
            }
        }
        return true;
    }
};

int main() {
    try {
        ChronologicalLayoutTester tester;
        tester.RunAllTests();
        
        std::cout << "\nAll tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}