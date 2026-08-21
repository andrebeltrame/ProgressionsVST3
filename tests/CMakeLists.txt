add_executable(harmonia_tests
    TestMain.cpp
    TestMidiFile.cpp
    TestTheory.cpp
    TestAnalysis.cpp
    TestGenerators.cpp
    TestProgression.cpp
    TestLibrary.cpp
    TestStyleModel.cpp
)

target_link_libraries(harmonia_tests PRIVATE harmonia_core)
target_include_directories(harmonia_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME harmonia_tests COMMAND harmonia_tests)
