# The VST3 / standalone build. Configure the top level with
#   -DHARMONIA_BUILD_PLUGIN=ON
# JUCE is picked up from external/JUCE if it is there, from -DJUCE_PATH=...,
# or downloaded with FetchContent as a last resort.

set(HARMONIA_JUCE_TAG "8.0.6" CACHE STRING "JUCE tag to build against")

if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Harmonia: using JUCE from ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" juce EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(STATUS "Harmonia: using JUCE from external/JUCE")
    add_subdirectory("${CMAKE_SOURCE_DIR}/external/JUCE" juce EXCLUDE_FROM_ALL)
else()
    message(STATUS "Harmonia: fetching JUCE ${HARMONIA_JUCE_TAG}")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${HARMONIA_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Harmonia
    PRODUCT_NAME            "Harmonia"
    COMPANY_NAME            "Harmonia Audio"
    BUNDLE_ID               "com.harmonia.ideagenerator"
    PLUGIN_MANUFACTURER_CODE Hrmn
    PLUGIN_CODE             Hrm1
    FORMATS                 VST3 Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${HARMONIA_INSTALL_PLUGIN}
    VST3_CATEGORIES         "Instrument" "Tools"
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(Harmonia PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/HarmoniaLookAndFeel.cpp
    Source/UI/PianoRollComponent.cpp
    Source/UI/ProgressionStrip.cpp
)

target_include_directories(Harmonia PRIVATE Source)

target_compile_definitions(Harmonia PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(Harmonia
    PRIVATE
        harmonia_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Headless checks for the plugin layer. Enabled together with the core tests.
if(HARMONIA_BUILD_TESTS)
    juce_add_console_app(harmonia_plugin_tests PRODUCT_NAME "harmonia_plugin_tests")
    target_sources(harmonia_plugin_tests PRIVATE tests/PluginSmokeTest.cpp)
    target_include_directories(harmonia_plugin_tests PRIVATE Source)
    target_compile_definitions(harmonia_plugin_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(harmonia_plugin_tests PRIVATE
        Harmonia
        harmonia_core
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_test(NAME harmonia_plugin_tests COMMAND harmonia_plugin_tests)
endif()
