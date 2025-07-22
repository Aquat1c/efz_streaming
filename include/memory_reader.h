#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <unordered_map>

class MemoryReader {
public:
    static bool Initialize();
    static void Shutdown();
    
    // Dynamic module detection
    static bool TryLoadEfzRevivalModule();
    static void StartModuleWatcher();
    static void StopModuleWatcher();
    
    // Memory read functions
    static bool ReadMemory(DWORD address, void* buffer, size_t size);
    static DWORD ReadDWORD(DWORD address);
    static int ReadByte(DWORD address);
    static std::string ReadString(DWORD address, size_t maxLength);  // Add this
    static std::wstring ReadWideString(DWORD address, size_t maxLength);
    
    // Game data accessors
    static int GetP1CharacterID();
    static int GetP2CharacterID();
    static DWORD GetP1WinCount();
    static DWORD GetP2WinCount();
    static std::wstring GetP1Nickname();
    static std::wstring GetP2Nickname();
    static std::string GetP1CharacterName();
    static std::string GetP2CharacterName();
    static std::string GetP1CharacterNameRaw();  // Add this
    static std::string GetP2CharacterNameRaw();  // Add this
    
private:
    static bool FindEFZProcess();
    static HMODULE GetModuleHandle(const std::string& moduleName);
    static HMODULE GetModuleHandleDynamic(const char* moduleName);
    static DWORD WINAPI ModuleWatcherThreadProc(LPVOID lpParam);
    static int MapRawCharacterNameToID(const std::string& rawName);
    static std::string GetCharacterNameFromID(int id);  // Add this line
    static bool IsValidNicknameChar(wchar_t c);
    static std::wstring SanitizeNickname(const std::wstring& nickname);
    static DWORD SanitizeWinCount(DWORD count);
    
    static HANDLE hProcess;
    static DWORD processId;
    static HMODULE efzModule;
    static HMODULE efzRevivalModule;
    static HANDLE moduleWatcherThread;
    static std::atomic<bool> watcherRunning;
    static const int MODULE_CHECK_INTERVAL_MS = 1000;

    // Add cache for memory reads to reduce repeated logs
    static std::unordered_map<DWORD, std::string> stringCache;
    static std::unordered_map<DWORD, int> byteCache;
    static std::unordered_map<DWORD, DWORD> dwordCache;
    static bool enableReadCache;
    
    // Helper for cache management
    static void ClearCache();
    static const int CACHE_CLEAR_INTERVAL_MS = 5000; // Clear cache every 5 seconds
    static DWORD lastCacheClearTime;
};
