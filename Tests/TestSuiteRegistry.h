#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

struct FTestSuite
{
    std::string Name;
    std::function<int()> Run;
};

class FTestSuiteRegistry
{
public:
    bool Register(std::string Name, std::function<int()> Run);

    [[nodiscard]] int Execute(
        const std::vector<std::string>& Arguments,
        std::ostream& Output,
        std::ostream& Error) const;

    [[nodiscard]] const std::vector<FTestSuite>& GetSuites() const noexcept;

private:
    std::vector<FTestSuite> Suites;
};
