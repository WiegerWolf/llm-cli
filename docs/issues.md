# GUI Graph View – Issue Backlog

Below is the backlog of technical issues identified during code review.  
Each entry references the source location and briefly explains impact.

1. **Hard-coded canvas center breaks large/resizeable viewports** – [`graph_manager.cpp:187-193`](graph_manager.cpp:187)  
   UpdateLayout always uses `ImVec2(1000, 750)`. On wider canvases the force simulation pulls everything toward this fixed point, causing off-screen layouts and poor UX.

2. **O(N²) repulsion loop stalls on big graphs** – [`graph_layout.cpp:253-299`](graph_layout.cpp:253)  
   `CalculateRepulsiveForces` iterates over every node pair each frame. 1 000 nodes → 500 000 force computations per iteration; no spatial partitioning (grid / Barnes-Hut).

3. **Expensive reseeding of RNG inside layout** – [`graph_layout.cpp:82-96`](graph_layout.cpp:82) and [`graph_layout.cpp:422-426`](graph_layout.cpp:422)  
   `std::random_device` and `std::mt19937` are re-constructed for every node; dominates CPU time and defeats deterministic replay.

4. **Duplicate random initialization on every layout restart** – [`graph_layout.cpp:76-98`](graph_layout.cpp:76)  
   Nodes already possessing valid positions are still randomly moved when `use_chronological_init == false`, producing visible “jumping” each refresh.

5. **ImGui::CalcTextSize used outside a frame/context** – [`graph_manager.cpp:24-31`](graph_manager.cpp:24)  
   `CalculateNodeSize` is called from data-layer functions that may run off the GUI thread; can crash when ImGui font atlas is unavailable.

6. **Unsafe raw-pointer containers** – [`graph_manager.h:19-22`](graph_manager.h:19)  
   `root_nodes` and child lists store bare `GraphNode*` pointing into `std::unique_ptr`s; any future deletion/pruning would create dangling pointers.

7. **Unbounded node-ID counter risk** – [`graph_manager.cpp:67-74`](graph_manager.cpp:67)  
   `next_graph_node_id_counter` resets only on full history reload; long sessions streaming thousands of messages may overflow 32-bit `NodeIdType`.

8. **Thread-safety absent for shared state** – [`graph_manager.cpp`](graph_manager.cpp:41) / [`graph_layout.cpp`](graph_layout.cpp:53)  
   `GraphManager` and `ForceDirectedLayout` are accessed by GUI and background threads without locks or atomic guards.

9. **Convergence gate hard-coded to 50 iterations** – [`graph_layout.cpp:125-133`](graph_layout.cpp:125)  
   Small graphs converge much earlier but still run 50 costly iterations every frame, degrading FPS.

10. **Vertical chronological bias contradiction** – [`graph_layout.cpp:482-493`](graph_layout.cpp:482)  
    Code attempts to place “earlier” messages lower on screen, but variable names/comments claim the opposite, causing temporal layout forces to conflict.

---

_End of backlog._
**[FIXED]** 2025-06-13 – Issue 2 resolved: uniform-grid spatial hashing replaces O(N²) repulsion loop in [`graph_layout.cpp`](graph_layout.cpp:253)