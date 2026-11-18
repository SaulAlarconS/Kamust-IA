cmake_minimum_required(VERSION 3.22)
project(KamustIA VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Audio Units exist only on macOS (required by Logic Pro). VST3 is built everywhere.
if(APPLE)
    set(KAMUST_FORMATS VST3 AU)
else()
    set(KAMUST_FORMATS VST3)
endif()

juce_add_plugin(Kamust_IA
    COMPANY_NAME "Kamust"
    PLUGIN_MANUFACTURER_CODE Kmst
    PLUGIN_CODE KmIA
    BUNDLE_ID com.kamust.kamustia
    PLUGIN_NAME "Kamust IA"
    PRODUCT_NAME "Kamust IA"
    FORMATS ${KAMUST_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_generate_juce_header(Kamust_IA)

target_sources(Kamust_IA PRIVATE
    Source/ResynthModel.cpp
    Source/ResynthVoice.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Kamust_IA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(Kamust_IA PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
