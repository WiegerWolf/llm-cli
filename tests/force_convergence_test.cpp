#include <cassert>
#include <vector>
#include <iostream>
#include "graph_layout.h"
#include "gui_interface/graph_types.h"

// Very small helper that builds a three-node linear chain and
// runs the force-directed solver until convergence.  The test
// passes if the solver stops in fewer than the legacy hard-coded
// 50 iterations (kMaxIterations upper-bound is still honoured).

int main() {
    // Trivial message objects (timestamp/content unused for this test).
    HistoryMessage dummy{};
    auto n1 = std::make_shared<GraphNode>(1, dummy);
    auto n2 = std::make_shared<GraphNode>(2, dummy);
    auto n3 = std::make_shared<GraphNode>(3, dummy);

    // Establish simple parent/child chain
    n1->add_child(n2);
    n2->parent = n1;
    n2->add_child(n3);
    n3->parent = n2;

    // Assign nominal sizes so forces are non-zero
    n1->size = n2->size = n3->size = ImVec2(200.0f, 80.0f);

    std::vector<GraphNode*> nodes = {n1.get(), n2.get(), n3.get()};

    ForceDirectedLayout layout;
    ImVec2 canvas_center(500.0f, 400.0f);
    layout.Initialize(nodes, canvas_center);

    int steps = 0;
    while (layout.UpdateLayout(nodes)) {
        ++steps;
        // Fail-safe to avoid infinite loop in case of regression
        if (steps > 1000) {
            std::cerr << "Simulation runaway\n";
            break;
        }
    }

    std::cout << "Iterations until convergence: " << steps << std::endl;
    assert(steps < 50 && "Solver should converge in fewer than kMaxIterations");

    // Clean-up

    std::cout << "force_convergence_test PASSED" << std::endl;
    return 0;
}