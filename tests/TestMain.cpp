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

int runAll(const std::string& filter)
{
    int failedTests = 0;
    int ran = 0;
    for (const auto& test : registry())
    {
        if (! filter.empty() && test.name.find(filter) == std::string::npos)
            continue;
        ++ran;
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

    if (ran == 0)
    {
        std::cout << "\nNo test matched '" << filter << "'\n";
        return 1;
    }

    std::cout << "\n" << ran - failedTests << "/" << ran << " tests passed";
    if (totalFailures > 0)
        std::cout << ", " << totalFailures << " failed checks";
    std::cout << "\n";
    return failedTests == 0 ? 0 : 1;
}

} // namespace testing

int main(int argc, char** argv)
{
    // An optional substring filter: harmonia_tests Progression
    return testing::runAll(argc > 1 ? argv[1] : "");
}
