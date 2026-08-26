cmake_minimum_required(VERSION 3.22)
project(CKStemSplitter VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CKStemSplitter
    COMPANY_NAME "Commercial Kings"
    BUNDLE_ID "com.commercialkings.ckstemsplitter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ckng
    PLUGIN_CODE Ckss
    FORMATS VST3 Standalone
    PRODUCT_NAME "CK Stem Splitter"
    PLUGIN_NAME "CK Stem Splitter"
)

juce_generate_juce_header(CKStemSplitter)

juce_add_plugin(CKAuditionOfflineProbe
    COMPANY_NAME "Commercial Kings"
    BUNDLE_ID "com.commercialkings.ckauditionofflineprobe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ckng
    PLUGIN_CODE Ckap
    FORMATS VST3
    PRODUCT_NAME "CK Stem Splitter"
    PLUGIN_NAME "CK Stem Splitter"
)

juce_generate_juce_header(CKAuditionOfflineProbe)

target_sources(CKAuditionOfflineProbe PRIVATE
    Probe/ProbeProcessor.cpp
    Probe/ProbeProcessor.h
)

target_compile_definitions(CKAuditionOfflineProbe PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CKAuditionOfflineProbe
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_sources(CKStemSplitter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StemEngine.cpp
    Source/StemEngine.h
)

target_compile_definitions(CKStemSplitter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CKStemSplitter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(WIN32)
    add_executable(CKVST3HostLoadTest
        tests/VST3HostLoadTest.cpp
    )

    target_compile_definitions(CKVST3HostLoadTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
    )

    target_link_libraries(CKVST3HostLoadTest PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )

    add_executable(CKAuditionOfflineProbeTest
        tests/AuditionOfflineProbeTest.cpp
    )

    target_include_directories(CKAuditionOfflineProbeTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Probe
        ${CMAKE_CURRENT_BINARY_DIR}/CKAuditionOfflineProbe_artefacts/JuceLibraryCode
    )

    target_link_libraries(CKAuditionOfflineProbeTest PRIVATE
        CKAuditionOfflineProbe
        juce::juce_audio_utils
    )

    add_executable(CKStemPrototypeRenderTest
        tests/StemPrototypeRenderTest.cpp
    )

    target_include_directories(CKStemPrototypeRenderTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Probe
        ${CMAKE_CURRENT_BINARY_DIR}/CKAuditionOfflineProbe_artefacts/JuceLibraryCode
    )

    target_link_libraries(CKStemPrototypeRenderTest PRIVATE
        CKAuditionOfflineProbe
        juce::juce_audio_formats
        juce::juce_audio_utils
    )

    add_executable(CKStemBridge WIN32
        Bridge/CKStemBridge.cpp
    )

    target_compile_definitions(CKStemBridge PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_link_libraries(CKStemBridge PRIVATE
        shell32
        user32
    )
endif()
