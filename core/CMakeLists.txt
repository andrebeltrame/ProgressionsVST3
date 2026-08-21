add_library(harmonia_core STATIC
    src/Types.cpp
    src/MidiFile.cpp
    src/Theory.cpp
    src/Analysis.cpp
    src/Generators.cpp
    src/Progression.cpp
    src/Presets.cpp
    src/Json.cpp
    src/Library.cpp
    src/Engine.cpp
)

target_include_directories(harmonia_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(harmonia_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(harmonia_core PRIVATE /W4)
else()
    target_compile_options(harmonia_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
