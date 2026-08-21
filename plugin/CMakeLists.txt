# The VST3 / standalone build. Configure the top level with
#   -DHARMONIA_BUILD_PLUGIN=ON
# JUCE is picked up from external/JUCE if it is there, from -DJUCE_PATH=...,
# or downloaded with FetchContent as a last resort.

set(HARMONIA_JUCE_TAG "8.0.6" CACHE STRING "JUCE tag to build against")

if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Progressions: using JUCE from ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" juce EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(STATUS "Progressions: using JUCE from external/JUCE")
    add_subdirectory("${CMAKE_SOURCE_DIR}/external/JUCE" juce EXCLUDE_FROM_ALL)
else()
    message(STATUS "Progressions: fetching JUCE ${HARMONIA_JUCE_TAG}")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${HARMONIA_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Installed plugins go into a vendor folder rather than loose in the VST3
# directory:  .../VST3/Nowhr Dynamics/Progressions.vst3
# The VST3 layout expects this and hosts scan the folder recursively, so it
# only keeps the user's plugin folder tidy - nothing has to be told where to
# look.
if(APPLE)
    set(HARMONIA_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Nowhr Dynamics")
elseif(WIN32)
    set(HARMONIA_VST3_DIR "$ENV{CommonProgramFiles}/VST3/Nowhr Dynamics")
else()
    set(HARMONIA_VST3_DIR "$ENV{HOME}/.vst3/Nowhr Dynamics")
endif()

juce_add_plugin(Progressions
    PRODUCT_NAME            "Progressions"
    COMPANY_NAME            "Nowhr Dynamics"
    BUNDLE_ID               "com.nowhrdynamics.progressions"
    PLUGIN_MANUFACTURER_CODE Nwhr
    PLUGIN_CODE             Prg1
    FORMATS                 VST3 Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${HARMONIA_INSTALL_PLUGIN}
    VST3_COPY_DIR           "${HARMONIA_VST3_DIR}"
    VST3_CATEGORIES         "Instrument" "Tools"
    # JUCE writes Contents/Resources/moduleinfo.json into the bundle as a
    # post-build step, after the bundle has been sealed. On macOS that leaves
    # the signature invalid ("a sealed resource is missing or invalid"), the
    # system refuses to load the plugin, and the host skips it without a word -
    # the plugin simply never appears in the list. The manifest only lets a host
    # enumerate classes without loading the module, so dropping it costs a
    # little scan speed and nothing else.
    VST3_AUTO_MANIFEST      FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(Progressions PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StyleStore.cpp
    Source/UI/ProgressionsLookAndFeel.cpp
    Source/UI/PianoRollComponent.cpp
    Source/UI/ProgressionStrip.cpp
)

# Bake a style model into the binary, so the .vst3 carries the brain with it:
#   -DHARMONIA_STYLE_MODEL=~/melodic.style.json
# Without it the plugin still picks up a model installed from the editor; this
# is for when the plugin itself has to be self-contained.
set(HARMONIA_STYLE_MODEL "" CACHE FILEPATH
    "A .style.json from 'harmonia-cli learn' to compile into the plugin")

if(HARMONIA_STYLE_MODEL)
    get_filename_component(HARMONIA_STYLE_MODEL_ABS "${HARMONIA_STYLE_MODEL}" ABSOLUTE)
    if(NOT EXISTS "${HARMONIA_STYLE_MODEL_ABS}")
        message(FATAL_ERROR "HARMONIA_STYLE_MODEL: no such file '${HARMONIA_STYLE_MODEL_ABS}'")
    endif()

    file(SIZE "${HARMONIA_STYLE_MODEL_ABS}" HARMONIA_STYLE_MODEL_BYTES)
    math(EXPR HARMONIA_STYLE_MODEL_KB "${HARMONIA_STYLE_MODEL_BYTES} / 1024")
    message(STATUS "Progressions: baking in ${HARMONIA_STYLE_MODEL_ABS} (${HARMONIA_STYLE_MODEL_KB} KB)")

    juce_add_binary_data(ProgressionsStyleData
        HEADER_NAME "ProgressionsStyleData.h"
        NAMESPACE ProgressionsStyleData
        SOURCES "${HARMONIA_STYLE_MODEL_ABS}")
    set_target_properties(ProgressionsStyleData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    target_link_libraries(Progressions PRIVATE ProgressionsStyleData)
    target_compile_definitions(Progressions PRIVATE HARMONIA_HAS_EMBEDDED_STYLE=1)
else()
    target_compile_definitions(Progressions PRIVATE HARMONIA_HAS_EMBEDDED_STYLE=0)
endif()

target_include_directories(Progressions PRIVATE Source)

target_compile_definitions(Progressions PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(Progressions
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
        Progressions
        harmonia_core
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_test(NAME harmonia_plugin_tests COMMAND harmonia_plugin_tests)
endif()
