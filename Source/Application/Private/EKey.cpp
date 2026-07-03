#include "Application/EKey.h"

namespace Stoner::Application
{

bool IsKnownKey(EKey Key) noexcept
{
    return Key != EKey::Unknown;
}

const char* ToString(EKey Key) noexcept
{
    switch (Key)
    {
    case EKey::Unknown: return "Unknown";
    case EKey::A: return "A";
    case EKey::B: return "B";
    case EKey::C: return "C";
    case EKey::D: return "D";
    case EKey::E: return "E";
    case EKey::F: return "F";
    case EKey::G: return "G";
    case EKey::H: return "H";
    case EKey::I: return "I";
    case EKey::J: return "J";
    case EKey::K: return "K";
    case EKey::L: return "L";
    case EKey::M: return "M";
    case EKey::N: return "N";
    case EKey::O: return "O";
    case EKey::P: return "P";
    case EKey::Q: return "Q";
    case EKey::R: return "R";
    case EKey::S: return "S";
    case EKey::T: return "T";
    case EKey::U: return "U";
    case EKey::V: return "V";
    case EKey::W: return "W";
    case EKey::X: return "X";
    case EKey::Y: return "Y";
    case EKey::Z: return "Z";
    case EKey::Num0: return "Num0";
    case EKey::Num1: return "Num1";
    case EKey::Num2: return "Num2";
    case EKey::Num3: return "Num3";
    case EKey::Num4: return "Num4";
    case EKey::Num5: return "Num5";
    case EKey::Num6: return "Num6";
    case EKey::Num7: return "Num7";
    case EKey::Num8: return "Num8";
    case EKey::Num9: return "Num9";
    case EKey::Escape: return "Escape";
    case EKey::Space: return "Space";
    case EKey::Enter: return "Enter";
    case EKey::Tab: return "Tab";
    case EKey::Backspace: return "Backspace";
    case EKey::Left: return "Left";
    case EKey::Right: return "Right";
    case EKey::Up: return "Up";
    case EKey::Down: return "Down";
    case EKey::Home: return "Home";
    case EKey::End: return "End";
    case EKey::PageUp: return "PageUp";
    case EKey::PageDown: return "PageDown";
    case EKey::Insert: return "Insert";
    case EKey::Delete: return "Delete";
    case EKey::LeftShift: return "LeftShift";
    case EKey::RightShift: return "RightShift";
    case EKey::LeftControl: return "LeftControl";
    case EKey::RightControl: return "RightControl";
    case EKey::LeftAlt: return "LeftAlt";
    case EKey::RightAlt: return "RightAlt";
    case EKey::F1: return "F1";
    case EKey::F2: return "F2";
    case EKey::F3: return "F3";
    case EKey::F4: return "F4";
    case EKey::F5: return "F5";
    case EKey::F6: return "F6";
    case EKey::F7: return "F7";
    case EKey::F8: return "F8";
    case EKey::F9: return "F9";
    case EKey::F10: return "F10";
    case EKey::F11: return "F11";
    case EKey::F12: return "F12";
    }
    return "Unknown";
}

} // namespace Stoner::Application
