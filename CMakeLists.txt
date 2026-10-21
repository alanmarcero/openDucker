cmake_minimum_required(VERSION 3.22)
project(openDucker VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
option(OD_BUILD_PLUGIN "Build VST3, AU and native tests" ON)
include(CTest)
add_library(ducker_core STATIC Source/Core/Ducker.cpp Source/Core/Effects.cpp)
target_include_directories(ducker_core PUBLIC Source/Core)
set_target_properties(ducker_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(BUILD_TESTING)
    add_executable(ducker_tests Tests/CoreTests.cpp)
    target_link_libraries(ducker_tests PRIVATE ducker_core)
    add_test(NAME core COMMAND ducker_tests)
    add_executable(ducker_effects_tests Tests/EffectsTests.cpp)
    target_link_libraries(ducker_effects_tests PRIVATE ducker_core)
    add_test(NAME effects COMMAND ducker_effects_tests)
endif()
if(OD_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
    juce_add_binary_data(DuckAssets SOURCES assets/dj-duck.png assets/analog-cyber-faceplate.png)
    set_target_properties(DuckAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(OD_FORMATS VST3)
    if(APPLE)
        list(APPEND OD_FORMATS AU)
    endif()
    juce_add_plugin(openDucker
        COMPANY_NAME "Alan Marcero"
        COMPANY_WEBSITE "https://github.com/alanmarcero/openDucker"
        COPYRIGHT "Copyright (c) 2026 Alan Marcero"
        BUNDLE_ID "com.alanmarcero.openducker"
        PRODUCT_NAME "openDucker"
        PLUGIN_MANUFACTURER_CODE Amrc PLUGIN_CODE Oduk
        FORMATS ${OD_FORMATS} VST3_CATEGORIES Fx Dynamics
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(openDucker PRIVATE Source/Plugin/PluginProcessor.cpp Source/Plugin/PluginEditor.cpp Source/Plugin/EffectsPanel.cpp)
    target_include_directories(openDucker PUBLIC Source/Plugin)
    target_compile_definitions(openDucker PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(openDucker PRIVATE ducker_core DuckAssets juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(BUILD_TESTING)
        add_executable(ducker_plugin_tests Tools/PluginTests.cpp)
        target_link_libraries(ducker_plugin_tests PRIVATE openDucker ducker_core DuckAssets juce::juce_audio_utils)
        target_compile_definitions(ducker_plugin_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        set_target_properties(ducker_plugin_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<CONFIG>")
        add_test(NAME plugin COMMAND ducker_plugin_tests "${CMAKE_BINARY_DIR}/screenshots")
    endif()
endif()
