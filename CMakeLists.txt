cmake_minimum_required(VERSION 3.22)
project(ReverseFX VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ReverseFX
    COMPANY_NAME "Sypnot"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Sypn
    PLUGIN_CODE Rvfx
    FORMATS VST3
    PRODUCT_NAME "ReverseFX")

target_sources(ReverseFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(ReverseFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ReverseFX
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)
