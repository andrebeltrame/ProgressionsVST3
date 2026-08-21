#pragma once

#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testing
{

struct TestCase
{
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& message);
/** Runs every registered test, or only those whose name contains `filter`. */
int runAll(const std::string& filter = {});

struct Registrar
{
    Registrar(const char* name, std::function<void()> body) { registry().push_back({ name, std::move(body) }); }
};

template <typename T>
std::string describe(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string describe(const std::vector<int>& values)
{
    std::ostringstream stream;
    stream << "[";
    for (size_t i = 0; i < values.size(); ++i)
        stream << (i > 0 ? ", " : "") << values[i];
    stream << "]";
    return stream.str();
}

} // namespace testing

#define TEST(name)                                                          \
    static void name();                                                     \
    static ::testing::Registrar registrar_##name(#name, name);              \
    static void name()

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (! (condition))                                                  \
            ::testing::reportFailure(__FILE__, __LINE__, "CHECK failed: " #condition); \
    } while (false)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto checkActual = (actual);                                  \
        const auto checkExpected = (expected);                              \
        if (! (checkActual == checkExpected))                               \
            ::testing::reportFailure(__FILE__, __LINE__,                    \
                std::string("CHECK_EQ failed: " #actual " == " #expected)   \
                + "\n      actual:   " + ::testing::describe(checkActual)   \
                + "\n      expected: " + ::testing::describe(checkExpected)); \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                             \
    do {                                                                    \
        const double checkActual = static_cast<double>(actual);             \
        const double checkExpected = static_cast<double>(expected);         \
        if (! (checkActual >= checkExpected - (tolerance)                   \
               && checkActual <= checkExpected + (tolerance)))              \
            ::testing::reportFailure(__FILE__, __LINE__,                    \
                std::string("CHECK_NEAR failed: " #actual)                  \
                + "\n      actual:   " + ::testing::describe(checkActual)   \
                + "\n      expected: " + ::testing::describe(checkExpected)); \
    } while (false)
