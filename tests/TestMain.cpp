#include "TestHarness.h"

#include <iostream>

namespace testing
{

namespace
{
int currentFailures = 0;
int totalFailures = 0;
}

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

void reportFailure(const char* file, int line, const std::string& message)
{
    ++currentFailures;
    ++totalFailures;
    std::cout << "    " << file << ":" << line << "  " << message << "\n";
}

int runAll()
{
    int failedTests = 0;
    for (const auto& test : registry())
    {
        currentFailures = 0;
        std::cout << "[ RUN  ] " << test.name << "\n";
        test.body();
        if (currentFailures == 0)
        {
            std::cout << "[  OK  ] " << test.name << "\n";
        }
        else
        {
            std::cout << "[ FAIL ] " << test.name << " (" << currentFailures << " checks)\n";
            ++failedTests;
        }
    }

    std::cout << "\n" << registry().size() - static_cast<size_t>(failedTests) << "/" << registry().size()
              << " tests passed";
    if (totalFailures > 0)
        std::cout << ", " << totalFailures << " failed checks";
    std::cout << "\n";
    return failedTests == 0 ? 0 : 1;
}

} // namespace testing

int main()
{
    return testing::runAll();
}
