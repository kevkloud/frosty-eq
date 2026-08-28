cmake_minimum_required(VERSION 3.22)
project(ClassicEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dev builds are native arm64. For a release universal binary, configure with
#   -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

add_subdirectory(libs/JUCE)

juce_add_plugin(ClassicEQ
    COMPANY_NAME                "Kloud Audio"
    BUNDLE_ID                   "com.kloudaudio.classiceq"
    PLUGIN_MANUFACTURER_CODE    Kld1
    PLUGIN_CODE                 Ceq1
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "ClassicEQ"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             "Fx" "EQ"
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(ClassicEQ PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(ClassicEQ PRIVATE src)

target_compile_definitions(ClassicEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Permitted: we are using JUCE under the AGPLv3 branch of its dual licence.
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ClassicEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
