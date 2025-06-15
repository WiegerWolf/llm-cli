#include <iostream>
#include <vector>
#include <cmath>
#include <core/id_types.h> // NodeIdType
#include <chrono>     // std::chrono utilities
#include <cstdlib>    // std::llabs
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
    bool TestLinearConversation() {
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
        
        auto& nodes = test_nodes_;
        
        // Test chronological initialization
        std::cout << "Testing chronological initialization..." << std::endl;
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        // Verify chronological ordering
        bool ok = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order maintained: " << (ok ? "PASS" : "FAIL") << std::endl;
        
        // Run force simulation
        std::cout << "\nRunning force simulation..." << std::endl;
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        // Verify chronological order is still maintained
        bool chronological_after_sim = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order after simulation: " << (chronological_after_sim ? "PASS" : "FAIL") << std::endl;
        ok &= chronological_after_sim;
        
        // Verify conversation structure
        bool structure_maintained = VerifyConversationStructure(nodes);
        std::cout << "Conversation structure maintained: " << (structure_maintained ? "PASS" : "FAIL") << std::endl;
        ok &= structure_maintained;

        return ok;
    }
    
    // Test Scenario 2: Branched conversation
    bool TestBranchedConversation() {
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
        
        auto& nodes = test_nodes_;
        
        std::cout << "Testing branched conversation layout..." << std::endl;
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        // Verify results
        bool ok = true;
        bool chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        ok &= chronological_order;
        
        bool structure_maintained = VerifyConversationStructure(nodes);
        std::cout << "Conversation structure: " << (structure_maintained ? "PASS" : "FAIL") << std::endl;
        ok &= structure_maintained;

        bool depth_positioning = VerifyDepthPositioning(nodes);
        std::cout << "Depth positioning: " << (depth_positioning ? "PASS" : "FAIL") << std::endl;
        ok &= depth_positioning;
        
        return ok;
    }
    
    // Test Scenario 3: Mixed chronological order input
    bool TestMixedChronologicalInput() {
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
        
        auto& nodes = test_nodes_;
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        
        // Verify that sorting worked correctly
        bool ok = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological sorting: " << (ok ? "PASS" : "FAIL") << std::endl;
        
        // Run simulation
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        bool chronological_after_sim = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order maintained: " << (chronological_after_sim ? "PASS" : "FAIL") << std::endl;
        ok &= chronological_after_sim;

        return ok;
    }
    
    // Test Scenario 4: Varying time gaps
    bool TestVaryingTimeGaps() {
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
        
        auto& nodes = test_nodes_;
        layout_.Initialize(nodes, ImVec2(750.0f, 600.0f));
        layout_.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
        
        // Verify chronological order and spacing
        bool ok = true;
        bool chronological_order = VerifyChronologicalOrder(nodes);
        std::cout << "Chronological order: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        ok &= chronological_order;

        bool appropriate_spacing = VerifyTimeGapSpacing(nodes);
        std::cout << "Appropriate time gap spacing: " << (appropriate_spacing ? "PASS" : "FAIL") << std::endl;
        ok &= appropriate_spacing;

        return ok;
    }
    
    // Test parameter validation
    bool TestParameterValidation() {
        std::cout << "\n=== Test 5: Parameter Validation ===" << std::endl;
        
        // Test different parameter combinations
        ForceDirectedLayout::LayoutParams params1 = CreateTestLayoutParams();
        params1.use_chronological_init = false;
        
        ForceDirectedLayout::LayoutParams params2 = CreateTestLayoutParams();
        params2.temporal_strength = 0.0f;
        
        ForceDirectedLayout::LayoutParams params3 = CreateTestLayoutParams();
        params3.vertical_bias = 1.0f;
        params3.chronological_spacing = 200.0f;
        
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
        
        auto& nodes = test_nodes_;
        
        // Test each parameter set
        bool ok = true;
        std::vector<ForceDirectedLayout::LayoutParams> param_sets = {params1, params2, params3};
        for (size_t i = 0; i < param_sets.size(); ++i) {
            ForceDirectedLayout test_layout(param_sets[i]);
            test_layout.Initialize(nodes, ImVec2(375.0f, 300.0f));
            test_layout.ComputeLayout(nodes, ImVec2(750.0f, 600.0f));
            
            bool chronological_order = VerifyChronologicalOrder(nodes);
            std::cout << "  Params " << (i + 1) << " result: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
            ok &= chronological_order;
        }
        return ok;
    }
    
    // Performance test
    bool TestPerformance() {
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
                nodes[i-1]->add_child(nodes[i]);
            }
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<std::shared_ptr<GraphNode>> raw_nodes;
        for(const auto& n : nodes) raw_nodes.push_back(n);
        // Initialize layout to reset velocities and ensure chronological placement
        layout_.Initialize(raw_nodes, ImVec2(750.0f, 600.0f));
        layout_.ComputeLayout(raw_nodes, ImVec2(750.0f, 600.0f));
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        bool ok = true;
        bool chronological_order = VerifyChronologicalOrder(raw_nodes);
        std::cout << "Chronological order maintained: " << (chronological_order ? "PASS" : "FAIL") << std::endl;
        ok &= chronological_order;
        
        // Performance benchmark
        bool performance_ok = duration.count() < 1000;
        std::cout << "Performance: " << (performance_ok ? "PASS" : "FAIL")
                  << " (" << duration.count() << "ms)" << std::endl;
        ok &= performance_ok;
        
        return ok;
    }
    
    // Run all tests
    bool RunAllTests() {
        std::cout << "=== Chronological Layout Algorithm Test Suite ===" << std::endl;
        bool all_tests_passed = true;
        
        all_tests_passed &= TestLinearConversation();
        all_tests_passed &= TestBranchedConversation();
        all_tests_passed &= TestMixedChronologicalInput();
        all_tests_passed &= TestVaryingTimeGaps();
        all_tests_passed &= TestParameterValidation();
        all_tests_passed &= TestPerformance();
        
        std::cout << "\n=== Test Suite Complete ===" << std::endl;
        if (all_tests_passed) {
            std::cout << "All tests passed." << std::endl;
        } else {
            std::cout << "Some tests failed." << std::endl;
        }
        return all_tests_passed;
    }

private:
    void ClearTestData() {
        test_nodes_.clear();
        layout_.ResetPhysicsState();
    }
    
    void PrintInitialPositions(const std::vector<std::shared_ptr<GraphNode>>&) {}
    void PrintNodePositions(const std::vector<std::shared_ptr<GraphNode>>&) {}
    
    bool VerifyChronologicalOrder(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        // Sort nodes by timestamp
        std::vector<std::shared_ptr<GraphNode>> sorted_nodes = nodes;
        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const auto& a, const auto& b) {
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
    
    bool VerifyConversationStructure(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        for (const auto& node : nodes) {
            // Check parent-child relationships are reasonable
            if (auto parent = node->parent.lock()) {
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
    
    bool VerifyDepthPositioning(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
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
    
    bool VerifyTimeGapSpacing(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        // Sort by timestamp
        std::vector<std::shared_ptr<GraphNode>> sorted_nodes = nodes;
        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const auto& a, const auto& b) {
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
    ChronologicalLayoutTester tester;
    bool ok = tester.RunAllTests();
    
    // Test interactive mode if enabled
#ifdef INTERACTIVE_TEST_MODE
    std::cout << "\nEntering interactive test mode. Close the window to exit." << std::endl;
    TestGui(tester); // The tester is passed to the GUI
#else
    //std::cout << "\nTo run in interactive mode, compile with -DINTERACTIVE_TEST_MODE" << std::endl;
#endif
    
    return ok ? 0 : 1;
}