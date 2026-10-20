cmake_minimum_required(VERSION 3.22)

project(QuantumSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Point this at an existing JUCE checkout to skip the download:
#   cmake -B build -DQS_JUCE_DIR=/path/to/JUCE
set(QS_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(QS_JUCE_DIR)
    add_subdirectory(${QS_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.16-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(QuantumSynth
    COMPANY_NAME "Quantum Synth"
    PRODUCT_NAME "Quantum Synth"
    BUNDLE_ID com.quantumsynth.QuantumSynth
    PLUGIN_MANUFACTURER_CODE Qntm
    PLUGIN_CODE Qsy1
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE)   # VST3 is copied to ~/.vst3, where Reaper looks

target_sources(QuantumSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(QuantumSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_JACK=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(QuantumSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Physics and DSP checks that run without JUCE: ctest or ./build/qs_dsp_tests
add_executable(qs_dsp_tests tools/dsp_tests.cpp)
enable_testing()
add_test(NAME dsp_tests COMMAND qs_dsp_tests ${CMAKE_BINARY_DIR}/quantum-demo.wav)
