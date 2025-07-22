#include "../include/memory_reader.h"
#include "../include/constants.h"
#include "../include/logger.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <cstring>
#include <thread>
#include <map>
#include <algorithm>
#include <unordered_map>
HANDLE MemoryReader::hProcess = nullptr;
DWORD MemoryReader::processId = 0;
HMODULE MemoryReader::efzModule = nullptr;
HMODULE MemoryReader::efzRevivalModule = nullptr;
HANDLE MemoryReader::moduleWatcherThread = nullptr;
std::atomic<bool> MemoryReader::watcherRunning(false);

// Initialize static cache variables
std::unordered_map<DWORD, std::string> MemoryReader::stringCache;
std::unordered_map<DWORD, int> MemoryReader::byteCache;
std::unordered_map<DWORD, DWORD> MemoryReader::dwordCache;
bool MemoryReader::enableReadCache = true;
DWORD MemoryReader::lastCacheClearTime = 0;

bool MemoryReader::Initialize() {
    LOG_FUNCTION_ENTRY();
    Logger::Info("Initializing memory reader");
    
    if (!FindEFZProcess()) {
        Logger::Error("Failed to find EFZ process");
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    // Get module handles
    efzModule = GetModuleHandle("efz.exe");
    
    if (!efzModule) {
        Logger::Error("Failed to get efz.exe module handle");
        LOG_FUNCTION_EXIT();
        return false;
    } else {
        Logger::Info("Found efz.exe module at " + Logger::FormatHex((DWORD)efzModule));
    }
    
    // Try to get EfzRevival.dll, but make it optional
    TryLoadEfzRevivalModule();
    
    // Check key memory addresses
    DWORD p1BaseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P1;
    DWORD p2BaseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P2;
    DWORD p1AddrValue = ReadDWORD(p1BaseAddr);
    DWORD p2AddrValue = ReadDWORD(p2BaseAddr);
    
    Logger::Info("P1 base address: " + Logger::FormatHex(p1BaseAddr) + 
                 " -> " + Logger::FormatHex(p1AddrValue));
    Logger::Info("P2 base address: " + Logger::FormatHex(p2BaseAddr) + 
                 " -> " + Logger::FormatHex(p2AddrValue));
                 
    Logger::Info("Memory reader initialized successfully");
    
    // Start watching for EfzRevival.dll if not found initially
    if (!efzRevivalModule) {
        Logger::Info("Starting background thread to monitor for EfzRevival.dll");
        StartModuleWatcher();
    }
    
    LOG_FUNCTION_EXIT();
    return true;
}

bool MemoryReader::TryLoadEfzRevivalModule() {
    // Try to get EfzRevival.dll
    if (!efzRevivalModule) {
        efzRevivalModule = GetModuleHandle("EfzRevival.dll");
        if (efzRevivalModule) {
            Logger::Info("Found EfzRevival.dll module at " + Logger::FormatHex((DWORD)efzRevivalModule));
            return true;
        } else {
            Logger::Warning("EfzRevival.dll not found - using vanilla EFZ memory layout");
            return false;
        }
    }
    return efzRevivalModule != nullptr;
}

// Replace the standalone thread function with the class member function
DWORD WINAPI MemoryReader::ModuleWatcherThreadProc(LPVOID lpParam) {
    while (watcherRunning) {
        if (TryLoadEfzRevivalModule()) {
            // Found the module, stop watching
            Logger::Info("EfzRevival.dll was loaded! Netplay/win count features now available.");
            break;
        }
        Sleep(MODULE_CHECK_INTERVAL_MS);
    }
    return 0;
}

void MemoryReader::StartModuleWatcher() {
    if (moduleWatcherThread == nullptr && !watcherRunning) {
        watcherRunning = true;
        moduleWatcherThread = CreateThread(NULL, 0, ModuleWatcherThreadProc, NULL, 0, NULL);
        if (moduleWatcherThread) {
            Logger::Info("Module watcher thread started");
        } else {
            LOG_WIN32_ERROR("Failed to start module watcher thread");
        }
    }
}

void MemoryReader::StopModuleWatcher() {
    if (moduleWatcherThread != nullptr && watcherRunning) {
        watcherRunning = false;
        // Wait for thread to terminate
        if (WaitForSingleObject(moduleWatcherThread, 1000) != WAIT_OBJECT_0) {
            TerminateThread(moduleWatcherThread, 0);
        }
        CloseHandle(moduleWatcherThread);
        moduleWatcherThread = nullptr;
        Logger::Info("Module watcher thread stopped");
    }
}

void MemoryReader::Shutdown() {
    LOG_FUNCTION_ENTRY();
    
    // Stop the module watcher thread if it's running
    StopModuleWatcher();
    
    if (hProcess != nullptr) {
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
    
    efzModule = nullptr;
    efzRevivalModule = nullptr;
    processId = 0;
    
    LOG_FUNCTION_EXIT();
}

bool MemoryReader::FindEFZProcess() {
    LOG_FUNCTION_ENTRY();
    PROCESSENTRY32 pe32;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR("CreateToolhelp32Snapshot failed");
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (!Process32First(hProcessSnap, &pe32)) {
        LOG_WIN32_ERROR("Process32First failed");
        CloseHandle(hProcessSnap);
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    Logger::Debug("Searching for efz.exe process");
    
    do {
        // Use strcmp with pe32.szExeFile which is char[], not wchar_t[]
        if (strcmp(pe32.szExeFile, "efz.exe") == 0) {
            processId = pe32.th32ProcessID;
            Logger::Info("Found efz.exe process with ID: " + std::to_string(processId));
            
            // Request both PROCESS_VM_READ and PROCESS_QUERY_INFORMATION access
            hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId);
            if (hProcess == nullptr) {
                LOG_WIN32_ERROR("OpenProcess failed");
            } else {
                Logger::Info("Successfully opened process handle: " + Logger::FormatHex((DWORD)hProcess));
            }
            
            CloseHandle(hProcessSnap);
            LOG_FUNCTION_EXIT();
            return hProcess != nullptr;
        }
    } while (Process32Next(hProcessSnap, &pe32));
    
    Logger::Error("EFZ process not found");
    CloseHandle(hProcessSnap);
    LOG_FUNCTION_EXIT();
    return false;
}

// In the GetModuleHandle method, make sure this line is fixed
HMODULE MemoryReader::GetModuleHandle(const std::string& moduleName) {
    LOG_FUNCTION_ENTRY();
    Logger::Debug("Getting module handle for: " + moduleName);
    
    HMODULE hMods[1024];
    DWORD cbNeeded;
    
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        DWORD numModules = cbNeeded / sizeof(HMODULE);
        Logger::Debug("Found " + std::to_string(numModules) + " modules in process");
        
        for (unsigned int i = 0; i < numModules; i++) {
            char szModName[MAX_PATH]; // Make sure this is char, not WCHAR
            
            if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                Logger::Debug("Module found: " + std::string(szModName) + " at " + Logger::FormatHex((DWORD)hMods[i]));
                
                if (moduleName == szModName) {
                    Logger::Info("Found target module: " + moduleName + " at " + Logger::FormatHex((DWORD)hMods[i]));
                    LOG_FUNCTION_EXIT();
                    return hMods[i];
                }
            }
        }
    } else {
        LOG_WIN32_ERROR("EnumProcessModules failed");
    }
    
    Logger::Error("Module not found: " + moduleName);
    LOG_FUNCTION_EXIT();
    return nullptr;
}

bool MemoryReader::ReadMemory(DWORD address, void* buffer, size_t size) {
    // Remove all debug output from this frequently called method
    
    SIZE_T bytesRead;
    bool result = ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, &bytesRead) && bytesRead == size;
    
    // Only log actual errors
    if (!result) {
        LOG_WIN32_ERROR("ReadProcessMemory failed at address " + Logger::FormatHex(address));
        Logger::LogMemoryOperation(address, "Read", result, size);
    }
    return result;
}

// Modified ReadString method to use cache
std::string MemoryReader::ReadString(DWORD address, size_t maxLength) {
    // Check if we need to clear cache (every 5 seconds)
    DWORD currentTime = GetTickCount();
    if (currentTime - lastCacheClearTime > CACHE_CLEAR_INTERVAL_MS) {
        ClearCache();
        lastCacheClearTime = currentTime;
    }
    
    // Check cache first if enabled
    if (enableReadCache) {
        auto it = stringCache.find(address);
        if (it != stringCache.end()) {
            return it->second;
        }
    }
    
    // Not in cache, read from memory
    char* buffer = new char[maxLength + 1];
    memset(buffer, 0, maxLength + 1);
    
    bool success = ReadMemory(address, buffer, maxLength);
    
    std::string result;
    if (success) {
        result = std::string(buffer);
        
        // Only cache non-empty results - THIS IS KEY
        if (enableReadCache && !result.empty()) {
            stringCache[address] = result;
        }
        
        // Only log non-empty strings that we haven't seen before
        if (!result.empty()) {
            static std::string lastNonEmptyString;
            static DWORD lastStringAddress = 0;
            
            if (result != lastNonEmptyString || address != lastStringAddress) {
                Logger::Debug("Reading string at " + Logger::FormatHex(address) + ": '" + result + "'");
                lastNonEmptyString = result;
                lastStringAddress = address;
            }
        }
    }
    
    delete[] buffer;
    return result;
}

// Add cache clearing function
void MemoryReader::ClearCache() {
    stringCache.clear();
    byteCache.clear();
    dwordCache.clear();
    Logger::Debug("Memory read cache cleared");
}

DWORD MemoryReader::ReadDWORD(DWORD address) {
    if (enableReadCache) {
        auto it = dwordCache.find(address);
        if (it != dwordCache.end()) {
            return it->second;
        }
    }
    
    DWORD value = 0;
    if (ReadMemory(address, &value, sizeof(DWORD))) {
        if (enableReadCache) {
            dwordCache[address] = value;
        }
    }
    return value;
}

int MemoryReader::ReadByte(DWORD address) {
    if (enableReadCache) {
        auto it = byteCache.find(address);
        if (it != byteCache.end()) {
            return it->second;
        }
    }
    
    BYTE value = 0;
    if (ReadMemory(address, &value, sizeof(BYTE))) {
        if (enableReadCache) {
            byteCache[address] = (int)value;
        }
    }
    return (int)value;
}

DWORD MemoryReader::GetP1WinCount() {
    // Remove these logging lines
    // LOG_FUNCTION_ENTRY();
    
    // Temporarily increase log level for this operation
    Logger::Level previousLevel = Logger::GetMinimumLevel();
    Logger::SetMinimumLevel(Logger::LOG_INFO);  // Only log INFO or higher
    
    DWORD rawWinCount = 0;
    
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P1_WIN_COUNT_OFFSET;
            rawWinCount = ReadDWORD(finalAddr);
            // Remove or comment out this debug log
            // Logger::Debug("Revival P1 Win Count: " + std::to_string(rawWinCount));
        }
    }
    
    // Restore previous log level
    Logger::SetMinimumLevel(previousLevel);
    
    // Remove this line
    // LOG_FUNCTION_EXIT();
    
    return SanitizeWinCount(rawWinCount); // Apply sanitization to the return value
}

DWORD MemoryReader::GetP2WinCount() {
    // Remove these logging lines
    // LOG_FUNCTION_ENTRY();
    
    // Temporarily increase log level
    Logger::Level previousLevel = Logger::GetMinimumLevel();
    Logger::SetMinimumLevel(Logger::LOG_INFO);
    
    DWORD result = 0;
    
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    if (efzRevivalModule) {
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P2_WIN_COUNT_OFFSET;
            result = ReadByte(finalAddr);
            // Remove or comment out this debug log
            // Logger::Debug("Revival P2 Win Count: " + std::to_string(result));
        }
    }
    
    // Restore previous log level
    Logger::SetMinimumLevel(previousLevel);
    
    // Remove this line
    // LOG_FUNCTION_EXIT();
    
    return SanitizeWinCount(result);
}

// Add these helper functions for data sanitization

bool MemoryReader::IsValidNicknameChar(wchar_t c) {
    // Allow standard alphanumeric
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
        return true;
    }
    
    // Common symbols for nicknames
    const wchar_t* allowedSymbols = L" -_.!?+#@$%&*()[]{}:;<>,'\"\\|/~^";
    for (size_t i = 0; allowedSymbols[i] != 0; i++) {
        if (c == allowedSymbols[i]) return true;
    }
    
    // Allow common CJK characters
    if ((c >= 0x3040 && c <= 0x30FF) ||    // Hiragana & Katakana
        (c >= 0x4E00 && c <= 0x9FFF) ||    // Common CJK
        (c >= 0xAC00 && c <= 0xD7AF)) {    // Hangul
        return true;
    }
    
    return false;
}

std::wstring MemoryReader::SanitizeNickname(const std::wstring& nickname) {
    std::wstring result;
    
    // If empty, return default
    if (nickname.empty()) {
        return L"Player";
    }
    
    // Maximum length check and sanitization
    size_t validChars = 0;
    for (size_t i = 0; i < nickname.length() && validChars < 16; i++) {
        if (IsValidNicknameChar(nickname[i])) {
            result += nickname[i];
            validChars++;
        }
        else {
            // Replace invalid characters with underscore
            if (!result.empty() && result.back() != L'_') {
                result += L'_';
                validChars++;
            }
        }
    }
    
    // If after sanitization we have nothing valid, return default
    if (result.empty()) {
        return L"Player";
    }
    
    return result;
}

// Sanitize win counts to prevent unreasonable values
DWORD MemoryReader::SanitizeWinCount(DWORD count) {
    // Keep win count in a reasonable range
    if (count > 99) {
        Logger::Warning("Suspicious win count detected: " + std::to_string(count));
        return 0;
    }
    return count;
}

// Updated nickname reader with proper sanitization
std::wstring MemoryReader::GetP1Nickname() {
    // Implementation depends on your memory reading logic
    std::wstring rawNickname;
    
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P1_NICKNAME_OFFSET;
            rawNickname = ReadWideString(finalAddr, MAX_NICKNAME_LENGTH);
        }
    }
    
    // Sanitize and provide default if needed
    return SanitizeNickname(rawNickname);
}

std::wstring MemoryReader::GetP2Nickname() {
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    std::wstring result;
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P2_NICKNAME_OFFSET;
            result = ReadWideString(finalAddr, MAX_NICKNAME_LENGTH);
        }
    } 
    // If empty or failed to read, use a default
    if (result.empty()) {
        result = L"Player 2";
    }
    
    return result;
}

// Replace these functions to use the string values instead of byte IDs
std::string MemoryReader::GetP1CharacterNameRaw() {
    // Get base address and read pointer
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P1;
    DWORD p1Addr = ReadDWORD(baseAddr);
    
    // Log pointer changes less frequently
    static DWORD lastP1Addr = 0;
    if (p1Addr != lastP1Addr) {
        Logger::Info("P1 base pointer: " + Logger::FormatHex(baseAddr) + 
                    " -> " + Logger::FormatHex(p1Addr));
        
        // Force cache clear when pointer changes - this is important for character selection
        if (p1Addr != 0 && lastP1Addr == 0) {
            ClearCache(); // Character was just selected, clear cache
        }
        
        lastP1Addr = p1Addr;
    }
    
    // If pointer is null, characters haven't been selected yet
    if (p1Addr == 0) {
        Logger::DebugThrottled("P1 character pointer is null - waiting for character selection", "p1ptr_null", 10000);
        return "";
    }
    
    // When reading character data during selection, temporarily disable caching
    static DWORD lastReadTime = GetTickCount();
    DWORD currentTime = GetTickCount();
    bool wasCacheEnabled = enableReadCache;
    
    // Disable cache for 1 second after pointer change to ensure fresh reads
    if (currentTime - lastReadTime < 1000) {
        enableReadCache = false;
    }
    
    // Read character name at pointer + offset
    std::string charName = ReadString(p1Addr + CHARACTER_NAME_OFFSET, 12);
    
    // Restore cache setting
    enableReadCache = wasCacheEnabled;
    
    // Only log changes to avoid spam
    static std::string lastP1Char = "";
    if (charName != lastP1Char) {
        if (!charName.empty()) {
            Logger::Info("Found P1 raw character name: '" + charName + "'");
            lastReadTime = currentTime; // Update last read time on character change
        }
        lastP1Char = charName;
    }
    
    return charName;
}

std::string MemoryReader::GetP2CharacterNameRaw() {
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P2;
    DWORD p2Addr = ReadDWORD(baseAddr);
    
    static DWORD lastP2Addr = 0;
    if (p2Addr != lastP2Addr) {
        Logger::Info("P2 base pointer: " + Logger::FormatHex(baseAddr) + 
                    " -> " + Logger::FormatHex(p2Addr));
        lastP2Addr = p2Addr;
    }
    
    if (p2Addr == 0) {
        Logger::DebugThrottled("P2 character pointer is null - waiting for character selection", "p2ptr_null", 10000);
        return "";
    }
    
    static std::string lastP2Char = "";
    std::string charName = ReadString(p2Addr + CHARACTER_NAME_OFFSET, 12);
    
    if (charName != lastP2Char) {
        if (!charName.empty()) {
            Logger::Info("Found P2 raw character name: '" + charName + "'");
        }
        lastP2Char = charName;
    }
    
    return charName;
}

// Optimize MapRawCharacterNameToID to avoid repetitive logging
int MemoryReader::MapRawCharacterNameToID(const std::string& rawName) {
    // Don't process empty names
    if (rawName.empty()) {
        return -1;
    }
    
    // Track only one instance of the character name per session
    static std::unordered_map<std::string, int> sessionNameCache;
    
    // Check session cache first for super-fast lookups
    auto cacheIt = sessionNameCache.find(rawName);
    if (cacheIt != sessionNameCache.end()) {
        return cacheIt->second;
    }
    
    // Convert to uppercase for case-insensitive comparison
    std::string upperName = rawName;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), 
                   [](unsigned char c) -> unsigned char { return static_cast<unsigned char>(std::toupper(c)); });
    
    // Define mapping from character names to IDs
    static const std::map<std::string, int> nameToIdMap = {
        {"AKANE", CHAR_ID_AKANE},
        {"AKIKO", CHAR_ID_AKIKO},
        {"IKUMI", CHAR_ID_IKUMI},
        {"MISAKI", CHAR_ID_MISAKI},
        {"SAYURI", CHAR_ID_SAYURI},
        {"KANNA", CHAR_ID_KANNA},
        {"KAORI", CHAR_ID_KAORI},
        {"MAKOTO", CHAR_ID_MAKOTO},
        {"MINAGI", CHAR_ID_MINAGI},
        {"MIO", CHAR_ID_MIO},
        {"MISHIO", CHAR_ID_MISHIO},
        {"MISUZU", CHAR_ID_MISUZU},
        {"MIZUKA", CHAR_ID_MIZUKA},
        {"NAGAMORI", CHAR_ID_NAGAMORI},
        {"NANASE", CHAR_ID_NANASE},
        {"EXNANASE", CHAR_ID_EXNANASE},
        {"NAYUKI", CHAR_ID_NAYUKI},
        {"NAYUKIB", CHAR_ID_NAYUKIB},
        {"SHIORI", CHAR_ID_SHIORI},
        {"AYU", CHAR_ID_AYU},
        {"MAI", CHAR_ID_MAI},
        {"MAYU", CHAR_ID_MAYU},
        {"MIZUKAB", CHAR_ID_MIZUKAB},
        {"KANO", CHAR_ID_KANO}
    };
    
    // First try exact match
    auto it = nameToIdMap.find(upperName);
    int result = -1;
    
    if (it != nameToIdMap.end()) {
        result = it->second;
    } else {
        // If no exact match, try prefix match
        for (const auto& pair : nameToIdMap) {
            if (upperName.find(pair.first) == 0) {
                result = pair.second;
                break;
            }
        }
    }
    
    // Log only the first time we see this character name
    if (result >= 0) {
        Logger::Info("Character detected: '" + rawName + "' -> ID " + std::to_string(result) +
                    " (" + GetCharacterNameFromID(result) + ")");
    } else {
        Logger::Warning("Unknown character name: '" + rawName + "'");
    }
    
    // Store in the session cache
    sessionNameCache[rawName] = result;
    
    return result;
}

// Now update the character ID getter functions
int MemoryReader::GetP1CharacterID() {
    std::string rawName = GetP1CharacterNameRaw();
    return MapRawCharacterNameToID(rawName);
}

int MemoryReader::GetP2CharacterID() {
    std::string rawName = GetP2CharacterNameRaw();
    return MapRawCharacterNameToID(rawName);
}

// Add this implementation for GetModuleHandleDynamic
HMODULE MemoryReader::GetModuleHandleDynamic(const char* moduleName) {
    HMODULE result = GetModuleHandleA(moduleName);
    if (result) {
        return result;
    }
    
    // If the above fails, try the more thorough approach
    return GetModuleHandle(std::string(moduleName));
}

// Add this helper function in the implementation (private section)
std::string MemoryReader::GetCharacterNameFromID(int id) {
    switch (id) {
        case CHAR_ID_AKANE: return "Akane";
        case CHAR_ID_AKIKO: return "Akiko";
        case CHAR_ID_IKUMI: return "Ikumi";
        case CHAR_ID_MISAKI: return "Misaki";
        case CHAR_ID_SAYURI: return "Sayuri";
        case CHAR_ID_KANNA: return "Kanna";
        case CHAR_ID_KAORI: return "Kaori";
        case CHAR_ID_MAKOTO: return "Makoto";
        case CHAR_ID_MINAGI: return "Minagi";
        case CHAR_ID_MIO: return "Mio";
        case CHAR_ID_MISHIO: return "Mishio";
        case CHAR_ID_MISUZU: return "Misuzu";
        case CHAR_ID_MIZUKA: return "UNKNOWN?!";
        case CHAR_ID_NAGAMORI: return "Mizuka";
        case CHAR_ID_NANASE: return "Rumi";
        case CHAR_ID_EXNANASE: return "Doppel Nanase";
        case CHAR_ID_NAYUKI: return "Nayuki(Asleep)";
        case CHAR_ID_NAYUKIB: return "Nayuki(Awake)";
        case CHAR_ID_SHIORI: return "Shiori";
        case CHAR_ID_AYU: return "Ayu";
        case CHAR_ID_MAI: return "Mai";
        case CHAR_ID_MAYU: return "Mayu";
        case CHAR_ID_MIZUKAB: return "UNKNOWN";
        case CHAR_ID_KANO: return "Kano";
        default: return "Undefined";
    }
}

// Add the public facing character name functions
std::string MemoryReader::GetP1CharacterName() {
    int characterId = GetP1CharacterID();
    return GetCharacterNameFromID(characterId);
}

std::string MemoryReader::GetP2CharacterName() {
    int characterId = GetP2CharacterID();
    return GetCharacterNameFromID(characterId);
}

std::wstring MemoryReader::ReadWideString(DWORD address, size_t maxLength) {
    // Check if we need to clear cache (every 5 seconds)
    DWORD currentTime = GetTickCount();
    if (currentTime - lastCacheClearTime > CACHE_CLEAR_INTERVAL_MS) {
        ClearCache();
        lastCacheClearTime = currentTime;
    }
    
    // Not using cache for wide strings yet to keep it simple
    
    // Read from memory
    wchar_t* buffer = new wchar_t[maxLength + 1];
    memset(buffer, 0, sizeof(wchar_t) * (maxLength + 1));
    
    bool success = ReadMemory(address, buffer, maxLength * sizeof(wchar_t));
    
    std::wstring result;
    if (success) {
        result = std::wstring(buffer);
        
        // Only log non-empty strings
        if (!result.empty()) {
            // For logging, just note that we read a wide string and its length
            // This avoids needing codecvt conversion for logging
            Logger::Debug("Reading wide string at " + Logger::FormatHex(address) + 
                " (length: " + std::to_string(result.length()) + " chars)");
        }
    }
    
    delete[] buffer;
    return result;
}