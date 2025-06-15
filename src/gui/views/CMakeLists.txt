add_library(gui_views OBJECT)

target_include_directories(gui_views
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gui_views PRIVATE core gui_render)

target_sources(gui_views PRIVATE
    main_gui_views.cpp
    gui_interface.cpp
)
