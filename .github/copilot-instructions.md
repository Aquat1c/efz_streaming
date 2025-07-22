<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

This is a C++ DLL project for creating an Eternal Fighter Zero streaming overlay mod. 

Key project details:
- Target: 32-bit Windows DLL for injection into efz.exe
- Purpose: Read game memory and provide data for OBS streaming overlays
- Architecture: Memory reader -> Data manager -> HTTP server + File output
- Memory targets: Win counts, player nicknames, character IDs from EFZ game process
- Build system: CMake with MSVC, requires Detours library for memory hooking

When working on this project:
- Maintain 32-bit compatibility (Win32 target)
- Use appropriate Windows API calls for process memory access
- Follow the existing logging pattern for debugging
- Keep HTTP responses lightweight for real-time streaming
- Remember that character IDs are 0-23 mapping to specific character names
