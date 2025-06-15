#!/usr/bin/env bash
set -e

# Phase 0 – create directory skeleton and minimal CMake stubs

dirs=(
"include/core"
"include/gui/render"
"include/gui/views"
"include/graph/layout"
"include/graph/render"
"include/graph/utils"
"include/db/drivers"
"include/db/stores"
"include/cli"
"include/utils"
"include/net"
"src/core"
"src/gui/render"
"src/gui/views"
"src/graph/layout"
"src/graph/render"
"src/graph/utils"
"src/db/drivers"
"src/db/stores"
"src/cli"
"src/utils"
"src/net"
"resources"
"tests"
)

for d in "${dirs[@]}"; do
  mkdir -p "$d"
done

# Mapping of leaf path -> logical library name
declare -A libs=(
  ["src/core"]="core"
  ["src/gui/render"]="gui_render"
  ["src/gui/views"]="gui_views"
  ["src/graph/layout"]="graph_layout"
  ["src/graph/render"]="graph_render"
  ["src/graph/utils"]="graph_utils"
  ["src/db/drivers"]="db_drivers"
  ["src/db/stores"]="db_stores"
  ["src/cli"]="cli"
  ["src/utils"]="utils"
  ["src/net"]="net"
)

for path in "${!libs[@]}"; do
  lib=${libs[$path]}
  cat > "$path/CMakeLists.txt" <<EOF
add_library(${lib} OBJECT)

target_include_directories(${lib}
    PUBLIC  \${PROJECT_SOURCE_DIR}/include
    PRIVATE \${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(${lib} PRIVATE ...)
EOF
done

# Append add_subdirectory() lines to the root CMakeLists.txt if absent
root_file="CMakeLists.txt"
for path in "${!libs[@]}"; do
  if ! grep -q "^add_subdirectory(${path})" "\$root_file"; then
    echo "add_subdirectory(${path})" >> "\$root_file"
  fi
done

if ! grep -q "^add_subdirectory(tests)" "\$root_file"; then
  echo "add_subdirectory(tests)" >> "\$root_file"
fi

echo "Phase 0 skeleton generated."