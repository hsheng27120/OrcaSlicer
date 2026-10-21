# widget_lab: standalone designer tool. Boundary guard - sources here must
# not include slic3r/GUI app headers (GUI.hpp / GUI_App.hpp / MainFrame.hpp /
# DeviceManager.hpp); same rule as the widgets library.
# Standalone widget lab: hosts WidgetInspectorDialog as the main window.
# Links ONLY the widgets static library (+ libslic3r for the data-dir
# helper) - the enforced boundary from the extraction change.
# Reuse the main application manifest: per-monitor-v2 DPI awareness and
# Common-Controls v6 - without it the lab renders blurry and unthemed.
if(NOT SLIC3R_VERSION)
    set(SLIC3R_VERSION "0.0.0.0")
endif ()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../dev-utils/platform/msw/Snapmaker_Orca.manifest.in
               ${CMAKE_CURRENT_BINARY_DIR}/widget_lab.manifest @ONLY)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/widget_lab.rc "1 24 \"widget_lab.manifest\"
")

set(WIDGET_LAB_SOURCES
    main.cpp
    WidgetInspectorDialog.cpp
    WidgetInspectorDialog.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/widget_lab.rc
)
add_executable(widget_lab WIN32 ${WIDGET_LAB_SOURCES})
target_include_directories(widget_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(widget_lab libslic3r_widgets libslic3r ${wxWidgets_LIBRARIES})
# The manifest is provided through widget_lab.rc - do not let the linker emit its own.
# MSVC-only flag; clang/lld would treat "/MANIFEST:NO" as an input file name.
if(MSVC)
    target_link_options(widget_lab PRIVATE "/MANIFEST:NO")
endif()
if(APPLE)
    # Dev convenience: the lab resolves resources relative to the executable
    # (Contents/MacOS). Link the source resources/ tree into the bundle so a
    # bare build-and-run finds icons without an install step.
    add_custom_command(TARGET widget_lab POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${CMAKE_SOURCE_DIR}/resources"
                "$<TARGET_FILE_DIR:widget_lab>/resources"
        COMMENT "Linking resources/ into widget_lab bundle")
endif()
if (SLIC3R_PCH AND NOT SLIC3R_SYNTAXONLY)
    add_precompiled_header(widget_lab ../slic3r/pchheader.hpp FORCEINCLUDE)
endif ()
