#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

namespace
{

bool ParseInteger(const char* Text, int& Out)
{
    const std::string_view View(Text == nullptr ? "" : Text);
    const auto Result = std::from_chars(
        View.data(), View.data() + View.size(), Out);
    return Result.ec == std::errc{} && Result.ptr == View.data() + View.size();
}

} // namespace

int main(int ArgumentCount, char* Arguments[])
{
    int ExitCode = 0;
    for (int Index = 1; Index < ArgumentCount; ++Index)
    {
        const std::string_view Argument(Arguments[Index]);
        if ((Argument == "--stdout" || Argument == "--stderr") &&
            Index + 1 < ArgumentCount)
        {
            std::ostream& Stream =
                Argument == "--stdout" ? std::cout : std::cerr;
            Stream << Arguments[++Index];
        }
        else if ((Argument == "--repeat-stdout" ||
                  Argument == "--repeat-stderr") &&
                 Index + 1 < ArgumentCount)
        {
            int Count = 0;
            if (!ParseInteger(Arguments[++Index], Count) || Count < 0)
            {
                return 64;
            }
            std::ostream& Stream =
                Argument == "--repeat-stdout" ? std::cout : std::cerr;
            for (int Byte = 0; Byte < Count; ++Byte)
            {
                Stream.put(Argument == "--repeat-stdout" ? 'O' : 'E');
            }
        }
        else if (Argument == "--sleep-ms" && Index + 1 < ArgumentCount)
        {
            int Milliseconds = 0;
            if (!ParseInteger(Arguments[++Index], Milliseconds) ||
                Milliseconds < 0)
            {
                return 64;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(Milliseconds));
        }
        else if (Argument == "--exit" && Index + 1 < ArgumentCount)
        {
            if (!ParseInteger(Arguments[++Index], ExitCode))
            {
                return 64;
            }
        }
        else
        {
            return 64;
        }
    }
    return ExitCode;
}
