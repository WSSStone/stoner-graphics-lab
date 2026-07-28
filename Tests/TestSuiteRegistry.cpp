#include "TestSuiteRegistry.h"

#include <algorithm>
#include <ostream>
#include <set>
#include <utility>

namespace
{

void PrintUsage(std::ostream& Error)
{
    Error << "Usage: StonerTest [--list-suites | --suite <name> ...]\n";
}

} // namespace

bool FTestSuiteRegistry::Register(std::string Name, std::function<int()> Run)
{
    if (Name.empty() || !Run)
    {
        return false;
    }

    const auto Existing = std::find_if(
        Suites.begin(),
        Suites.end(),
        [&Name](const FTestSuite& Suite) { return Suite.Name == Name; });
    if (Existing != Suites.end())
    {
        return false;
    }

    Suites.push_back({std::move(Name), std::move(Run)});
    std::sort(
        Suites.begin(),
        Suites.end(),
        [](const FTestSuite& Left, const FTestSuite& Right)
        {
            return Left.Name < Right.Name;
        });
    return true;
}

int FTestSuiteRegistry::Execute(
    const std::vector<std::string>& Arguments,
    std::ostream& Output,
    std::ostream& Error) const
{
    if (Arguments.size() == 1 && Arguments[0] == "--list-suites")
    {
        for (const FTestSuite& Suite : Suites)
        {
            Output << Suite.Name << '\n';
        }
        return 0;
    }

    std::set<std::string> Selected;
    if (!Arguments.empty())
    {
        for (std::size_t Index = 0; Index < Arguments.size();)
        {
            if (Arguments[Index] != "--suite")
            {
                Error << "Unknown argument: " << Arguments[Index] << '\n';
                PrintUsage(Error);
                return 2;
            }
            if (Index + 1 >= Arguments.size())
            {
                Error << "Missing suite name after --suite\n";
                PrintUsage(Error);
                return 2;
            }

            const std::string& Name = Arguments[Index + 1];
            if (Name == "all")
            {
                Selected.clear();
                for (const FTestSuite& Suite : Suites)
                {
                    Selected.insert(Suite.Name);
                }
            }
            else
            {
                const auto Found = std::find_if(
                    Suites.begin(),
                    Suites.end(),
                    [&Name](const FTestSuite& Suite) { return Suite.Name == Name; });
                if (Found == Suites.end())
                {
                    Error << "Unknown suite: " << Name << '\n';
                    PrintUsage(Error);
                    return 2;
                }
                Selected.insert(Name);
            }
            Index += 2;
        }
    }
    else
    {
        for (const FTestSuite& Suite : Suites)
        {
            Selected.insert(Suite.Name);
        }
    }

    int ExitCode = 0;
    for (const FTestSuite& Suite : Suites)
    {
        if (Selected.contains(Suite.Name) && Suite.Run() != 0)
        {
            ExitCode = 1;
        }
    }
    return ExitCode;
}

const std::vector<FTestSuite>& FTestSuiteRegistry::GetSuites() const noexcept
{
    return Suites;
}
