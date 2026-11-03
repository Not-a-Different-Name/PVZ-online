# Configure a separate MOD110 native build with
# -DCMAKE_PROJECT_INCLUDE=/absolute/path/to/tools/cli/cli.cmake
# Keep the original framework, resources and dependencies; replace only its entry.
if(CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR "The CLI adapter requires CMake 3.19 or newer")
endif()
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    return()
endif()
set_property(GLOBAL PROPERTY PVZ_CLI_ADAPTER_SOURCE "${CMAKE_CURRENT_LIST_DIR}/engine.cpp")
function(pvz_cli_attach)
    if(NOT TARGET LawnProject)
        message(FATAL_ERROR "This adapter requires the MOD110 LawnProject target")
    endif()
    get_target_property(pvz_cli_sources LawnProject SOURCES)
    list(FILTER pvz_cli_sources EXCLUDE REGEX "(^|[/\\\\])main[.]cpp$")
    set_property(TARGET LawnProject PROPERTY SOURCES "${pvz_cli_sources}")
    get_property(pvz_cli_entry GLOBAL PROPERTY PVZ_CLI_ADAPTER_SOURCE)
    target_sources(LawnProject PRIVATE "${pvz_cli_entry}")
    set_target_properties(LawnProject PROPERTIES WIN32_EXECUTABLE FALSE
        OUTPUT_NAME "pvz-cli-engine" TARGET_NAME_RELEASE "pvz-cli-engine"
        TARGET_NAME_DEBUG "pvz-cli-engine" RUNTIME_OUTPUT_NAME_RELEASE "pvz-cli-engine"
        RUNTIME_OUTPUT_NAME_DEBUG "pvz-cli-engine")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL pvz_cli_attach)
