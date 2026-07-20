#include "FDemoConfiguration.h"
#include "FStonerDemoApplication.h"

#include <iostream>

int main(int ArgCount, const char* const* Arguments)
{
    Stoner::Demo::FDemoConfiguration Configuration;
    Stoner::Core::FString Reason;
    const Stoner::Demo::EDemoExitCode ParseResult =
        Stoner::Demo::FDemoConfiguration::Parse(ArgCount, Arguments, Configuration, Reason);
    if (ParseResult != Stoner::Demo::EDemoExitCode::Success)
    {
        std::cerr << "configuration-error: " << Reason.CStr() << '\n';
        return static_cast<int>(ParseResult);
    }

    Stoner::Demo::FStonerDemoApplication Application(Configuration);
    const Stoner::Demo::EDemoExitCode Result = Application.Run();
    std::cout << Application.GetDiagnostics().BuildStableText().CStr();
    return static_cast<int>(Result);
}
