# Dear ImGui private dependency

Version: v1.92.5. Commit: `6d910d5487d11ca567b61c7824b0c78c569d62f0`.
Source: https://github.com/ocornut/imgui/tree/6d910d5487d11ca567b61c7824b0c78c569d62f0
Archive: https://codeload.github.com/ocornut/imgui/tar.gz/6d910d5487d11ca567b61c7824b0c78c569d62f0

The 13 files in manifest.json are copied byte-for-byte from this archive;
per-file sizes/SHA-256 and archive SHA-256 record the downloaded bytes.
Only imgui.cpp, imgui_draw.cpp, imgui_tables.cpp and imgui_widgets.cpp build.
Configuration belongs to Application/Private/ImGuiUserConfig.h; no upstream
source is patched. No platform/render backend or demonstration source is vendored.

Cousine-Regular.ttf comes from the same commit. Its name records identify
2010 Google Corporation copyright and SIL Open Font License 1.1. docs/FONTS.md
is the unmodified upstream font attribution; LICENSE.txt covers core MIT code.
FONT-LICENSE.txt retains that fixed binary copyright followed by the standard
OFL 1.1 text obtained from Google Fonts; manifest.json records this supplemental
text separately, without attributing it to the pinned source archive. No font
modification or renaming is performed. The build generates an immutable font
array from the fixed binary; runtime filesystem font loading is disabled.
