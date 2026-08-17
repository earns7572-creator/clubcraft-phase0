cmake_minimum_required(VERSION 3.22)

project(ClubCraftPhase0 VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CLUBCRAFT_BUILD_TESTS "Build Club Craft Phase 0 tests" ON)
option(CLUBCRAFT_BUILD_AU "Build the macOS Audio Unit format alongside VST3" OFF)

set(CLUBCRAFT_PLUGIN_FORMATS VST3)
if(APPLE AND CLUBCRAFT_BUILD_AU)
    list(APPEND CLUBCRAFT_PLUGIN_FORMATS AU)
endif()

add_subdirectory(JUCE)

add_library(ClubCraftCore STATIC
    src/common/SessionRegistry.cpp
    src/common/SceneCompiler.cpp
    src/common/SceneState.cpp
)

target_include_directories(ClubCraftCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(ClubCraftCore PUBLIC juce::juce_data_structures)

target_compile_features(ClubCraftCore PUBLIC cxx_std_20)
set_target_properties(ClubCraftCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(ClubCraftCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    PRIVATE
        $<$<CONFIG:Debug>:CLUBCRAFT_DEBUG_BUILD=1>
)

# A single binary is intentional. In many hosts, separate VST3 modules may be
# loaded into separate address spaces, making static process-local state unsafe
# for Source/Club communication. Each inserted instance chooses its SOURCE or
# CLUB role through a non-automatable state parameter.
juce_add_plugin(ClubCraft
    COMPANY_NAME "Club Craft"
    BUNDLE_ID "im.clubcraft.phase0"
    PLUGIN_MANUFACTURER_CODE CCft
    PLUGIN_CODE CCp0
    FORMATS ${CLUBCRAFT_PLUGIN_FORMATS}
    PRODUCT_NAME "Club Craft"
    PLUGIN_NAME "Club Craft"
    DESCRIPTION "Virtual club routing prototype"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx"
)

target_sources(ClubCraft
    PRIVATE
        src/club/PluginProcessor.cpp
        src/club/PluginProcessor.h
        src/club/PluginEditor.cpp
        src/club/PluginEditor.h
)

target_link_libraries(ClubCraft
    PRIVATE
        ClubCraftCore
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(ClubCraft
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        JUCE_REPORT_APP_USAGE=0
)

if(CLUBCRAFT_BUILD_TESTS)
    enable_testing()

    add_executable(ClubCraftCoreTests
        tests/SessionRegistryTests.cpp
        tests/SceneCompilerTests.cpp
        tests/SourceRouteRendererTests.cpp
    )

    target_link_libraries(ClubCraftCoreTests
        PRIVATE
            ClubCraftCore
            juce::juce_dsp
    )
    target_compile_features(ClubCraftCoreTests PRIVATE cxx_std_20)
    target_compile_definitions(ClubCraftCoreTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    add_test(NAME ClubCraftCoreTests COMMAND ClubCraftCoreTests)
endif()
