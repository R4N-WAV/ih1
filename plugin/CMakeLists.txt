# IH1 VST3 plug-in (JUCE 9 wrapper around ih1_core). Licence note: JUCE is used under the JUCE 9 EULA
# (Starter tier) - see docs/OPEN_DECISIONS.md (A1) and README.md.

set(IH1_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout (otherwise fetched at the pinned commit)")
if(IH1_JUCE_DIR)
    add_subdirectory(${IH1_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd  # 9.0.5
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(juce)
endif()

set(IH1_FORMATS VST3)
option(IH1_STANDALONE "Also build a standalone app (for testing)" ON)
if(IH1_STANDALONE)
    list(APPEND IH1_FORMATS Standalone)
endif()

juce_add_plugin(IH1
    PRODUCT_NAME "IH1"
    COMPANY_NAME "R4N"
    BUNDLE_ID com.r4n.ih1
    PLUGIN_MANUFACTURER_CODE R4nW
    PLUGIN_CODE Ih1x
    FORMATS ${IH1_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    VERSION ${PROJECT_VERSION})

target_sources(IH1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Ih1LookAndFeel.cpp
    Source/Carousel.cpp
    Source/SettingsPanel.cpp
    Source/FormationView.cpp
    Source/Widgets.cpp
    Source/FxGraph.cpp
    Source/PresetBrowser.cpp)

# Build identity shown in the editor header (v<version> · <commit>), so a user can always tell which build
# a host has loaded.
set(IH1_GIT_HASH "dev")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE IH1_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT IH1_GIT_HASH)
        set(IH1_GIT_HASH "dev")
    endif()
endif()

target_compile_definitions(IH1 PUBLIC
    IH1_VERSION_STRING="${PROJECT_VERSION}"
    IH1_GIT_HASH="${IH1_GIT_HASH}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(IH1
    PRIVATE
        ih1_core
        juce::juce_audio_utils
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Host-lifecycle test host (loads the built VST3; not shipped)
option(IH1_BUILD_HOSTTEST "Build the ih1_hosttest VST3 host-lifecycle checker" ON)
if(IH1_BUILD_HOSTTEST)
    juce_add_console_app(ih1_hosttest PRODUCT_NAME "ih1_hosttest")
    target_sources(ih1_hosttest PRIVATE ${CMAKE_SOURCE_DIR}/tools/hosttest/HostTest.cpp)
    target_compile_definitions(ih1_hosttest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ih1_hosttest PRIVATE juce::juce_audio_processors juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# UI interaction test (drives the real editor through its peer; needs a display). Not shipped.
option(IH1_BUILD_UITEST "Build ih1_uitest" ON)
if(IH1_BUILD_UITEST)
    juce_add_console_app(ih1_uitest PRODUCT_NAME "ih1_uitest")
    target_sources(ih1_uitest PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/uitest/UiTest.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Ih1LookAndFeel.cpp Source/Carousel.cpp
        Source/SettingsPanel.cpp Source/FormationView.cpp Source/Widgets.cpp Source/FxGraph.cpp Source/PresetBrowser.cpp)
    target_compile_definitions(ih1_uitest PRIVATE IH1_VERSION_STRING="${PROJECT_VERSION}" IH1_GIT_HASH="${IH1_GIT_HASH}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="IH1" IH1_UITEST=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ih1_uitest PRIVATE ih1_core juce::juce_audio_utils juce::juce_opengl
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
