cmake_minimum_required(VERSION 3.22)

project(IH1 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(IH1_BUILD_TESTS "Build ih1_core unit tests" ON)
option(IH1_BUILD_PLUGIN "Build the JUCE VST3 plug-in" ON)
option(IH1_BUILD_TOOLS "Build offline tools (renders, formation preview)" ON)

include(FetchContent)

# ---------------------------------------------------------------------------- core
file(GLOB_RECURSE IH1_CORE_SOURCES CONFIGURE_DEPENDS core/src/*.cpp)
add_library(ih1_core STATIC ${IH1_CORE_SOURCES})
target_include_directories(ih1_core PUBLIC core/include)
if(MSVC)
    target_compile_options(ih1_core PRIVATE /W4 /permissive- /fp:fast)
else()
    target_compile_options(ih1_core PRIVATE -Wall -Wextra -Wno-unused-parameter -ffp-contract=fast)
endif()

# ---------------------------------------------------------------------------- tests
if(IH1_BUILD_TESTS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
    enable_testing()
    file(GLOB IH1_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(ih1_tests ${IH1_TEST_SOURCES})
    target_link_libraries(ih1_tests PRIVATE ih1_core doctest::doctest)
    target_compile_definitions(ih1_tests PRIVATE _USE_MATH_DEFINES)
    add_test(NAME ih1_tests COMMAND ih1_tests)
endif()

# ---------------------------------------------------------------------------- tools
if(IH1_BUILD_TOOLS)
    file(GLOB IH1_TOOL_SOURCES CONFIGURE_DEPENDS tools/*.cpp)
    foreach(src ${IH1_TOOL_SOURCES})
        get_filename_component(name ${src} NAME_WE)
        add_executable(${name} ${src})
        target_link_libraries(${name} PRIVATE ih1_core)
        target_compile_definitions(${name} PRIVATE _USE_MATH_DEFINES)
    endforeach()
endif()

# ---------------------------------------------------------------------------- plugin
if(IH1_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
