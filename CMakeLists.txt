cmake_minimum_required(VERSION 3.22)

project(NeonRift VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing. Run: python tools/fetch_deps.py")
endif()

add_subdirectory(external/JUCE)

#==============================================================================
# The plugin (shared code + VST3 + Standalone)
#==============================================================================
juce_add_plugin(NeonRift
    COMPANY_NAME                "NeonRift Audio"
    BUNDLE_ID                   "com.neonrift.synth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Nrft
    PLUGIN_CODE                 Nr01
    PRODUCT_NAME                "NeonRift"
    VERSION                     "1.0.0"
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             "Instrument" "Synth")

target_sources(NeonRift PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthEngine.cpp)

target_compile_definitions(NeonRift PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(NeonRift
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Offline render harness: proves the engine makes sound without a DAW.
#==============================================================================
juce_add_console_app(NeonRiftRender PRODUCT_NAME "NeonRiftRender")

target_sources(NeonRiftRender PRIVATE
    tests/RenderTest.cpp
    Source/SynthEngine.cpp)

target_include_directories(NeonRiftRender PRIVATE Source)

target_compile_definitions(NeonRiftRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=0)

target_link_libraries(NeonRiftRender
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags)

#==============================================================================
# Editor harness: builds the real editor, snapshots it, tears it down.
#==============================================================================
juce_add_console_app(NeonRiftEditorTest PRODUCT_NAME "NeonRiftEditorTest")

target_sources(NeonRiftEditorTest PRIVATE
    tests/EditorTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthEngine.cpp)

target_include_directories(NeonRiftEditorTest PRIVATE Source)

target_compile_definitions(NeonRiftEditorTest PRIVATE
    NEONRIFT_TRACE=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NeonRiftEditorTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags)
