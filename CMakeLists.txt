cmake_minimum_required(VERSION 3.22)

project(Exciter VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Exciter
    COMPANY_NAME                "Songcraft Audio"
    BUNDLE_ID                   com.songcraftaudio.exciter
    PLUGIN_MANUFACTURER_CODE    Sngc
    PLUGIN_CODE                 Exct
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Sparkular"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

target_sources(Exciter PRIVATE
    ExciterProcessor.cpp
    ExciterProcessor.h
    PluginEditor.cpp
    PluginEditor.h
)

target_compile_definitions(Exciter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Exciter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
