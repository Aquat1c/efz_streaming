#include "../include/memory_reader.h"
#include "../include/constants.h"
#include "../include/logger.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <cstring>
#include <thread>
#include <map>
#include <algorithm>
HANDLE MemoryReader::hProcess = nullptr;
DWORD MemoryReader::processId = 0;
HMODULE MemoryReader::efzModule = nullptr;
HMODULE MemoryReader::efzRevivalModule = nullptr;
HANDLE MemoryReader::moduleWatcherThread = nullptr;
std::atomic<bool> MemoryReader::watcherRunning(false);

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

// In GetModuleHandle method, ensure szModName is the correct type
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
    // Remove this debug line that's causing most of the spam
    //#ifdef VERBOSE_LOGGING
    //Logger::Debug("Reading " + std::to_string(size) + " bytes from " + Logger::FormatHex(address));
    //#endif
    
    SIZE_T bytesRead;
    bool result = ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, &bytesRead) && bytesRead == size;
    
    if (!result) {
        LOG_WIN32_ERROR("ReadProcessMemory failed at address " + Logger::FormatHex(address));
    }
    
    // Only log errors, not successful operations
    if (!result) {
        Logger::LogMemoryOperation(address, "Read", result, size);
    }
    return result;
}

DWORD MemoryReader::ReadDWORD(DWORD address) {
    DWORD value = 0;
    if (ReadMemory(address, &value, sizeof(DWORD))) {
        // Remove verbose logging of every DWORD read
        //Logger::Debug("ReadDWORD at " + Logger::FormatHex(address) + " = " + Logger::FormatHex(value));
    }
    return value;
}

std::wstring MemoryReader::ReadWideString(DWORD address, size_t maxLength) {
    Logger::Debug("Reading wide string at " + Logger::FormatHex(address) + " (max length: " + std::to_string(maxLength) + ")");
    
    wchar_t* buffer = new wchar_t[maxLength + 1];
    memset(buffer, 0, (maxLength + 1) * sizeof(wchar_t));
    
    bool success = ReadMemory(address, buffer, maxLength * sizeof(wchar_t));
    
    std::wstring result;
    if (success) {
        result = std::wstring(buffer);
        
        // Convert to narrow for logging
        std::string narrowStr;
        for (size_t i = 0; i < result.length(); i++) {
            narrowStr += static_cast<char>(result[i] & 0xFF);
        }
        Logger::Debug("String value: " + narrowStr);
    }
    
    delete[] buffer;
    return result;
}

// Add this function to read string data from memory
std::string MemoryReader::ReadString(DWORD address, size_t maxLength) {
    Logger::Debug("Reading string at " + Logger::FormatHex(address) + " (max length: " + std::to_string(maxLength) + ")");
    
    char* buffer = new char[maxLength + 1];
    memset(buffer, 0, maxLength + 1);
    
    bool success = ReadMemory(address, buffer, maxLength);
    
    std::string result;
    if (success) {
        result = std::string(buffer);
        Logger::Debug("String value: " + result);
    }
    
    delete[] buffer;
    return result;
}

int MemoryReader::ReadByte(DWORD address) {
    BYTE value = 0;
    if (ReadMemory(address, &value, sizeof(BYTE))) {
        // Remove verbose logging of every byte read
        //Logger::Debug("ReadByte at " + Logger::FormatHex(address) + " = " + std::to_string((int)value));
    }
    return (int)value;
}

DWORD MemoryReader::GetP1WinCount() {
    LOG_FUNCTION_ENTRY();
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
            Logger::Debug("Revival P1 Win Count: " + std::to_string(rawWinCount));
        }
    }
    
    LOG_FUNCTION_EXIT();
    return SanitizeWinCount(rawWinCount); // Apply sanitization to the return value
}

DWORD MemoryReader::GetP2WinCount() {
    LOG_FUNCTION_ENTRY();
    DWORD result = 0;
    
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P2_WIN_COUNT_OFFSET;
            result = ReadByte(finalAddr); // P2 win count is stored as byte in Revival
            Logger::Debug("Revival P2 Win Count: " + std::to_string(result));
        }
    }
    
    LOG_FUNCTION_EXIT();
    return result;
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
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P1;
    DWORD p1Addr = ReadDWORD(baseAddr);
    if (p1Addr == 0) return "";
    return ReadString(p1Addr + CHARACTER_NAME_OFFSET, 12); // Use the 12-byte string length from CheatTable
}

std::string MemoryReader::GetP2CharacterNameRaw() {
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P2;
    DWORD p2Addr = ReadDWORD(baseAddr);
    if (p2Addr == 0) return "";
    return ReadString(p2Addr + CHARACTER_NAME_OFFSET, 12); // Use the 12-byte string length from CheatTable
}

// Replace the MapRawCharacterNameToID function with this version
int MemoryReader::MapRawCharacterNameToID(const std::string& rawName) {
    // Don't log every mapping attempt
    static std::string lastRawName;
    static int lastResult = -1;
    
    // Make sure to also handle partial names and variations
    static const std::map<std::string, int> nameToIdMap = {
        // Keep existing mappings
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
        {"KANO", CHAR_ID_KANO},
        
        // Common variations (partial name detection)
        {"AKANE_", CHAR_ID_AKANE},
        {"AKIKO_", CHAR_ID_AKIKO},
        {"IKUMI_", CHAR_ID_IKUMI},
        {"MISAKI_", CHAR_ID_MISAKI},
        {"SAYURI_", CHAR_ID_SAYURI},
        {"KANNA_", CHAR_ID_KANNA},
        {"KAORI_", CHAR_ID_KAORI},
        {"MAKOTO_", CHAR_ID_MAKOTO},
        {"MINAGI_", CHAR_ID_MINAGI},
        {"MIO_", CHAR_ID_MIO},
        {"MISHIO_", CHAR_ID_MISHIO},
        {"MISUZU_", CHAR_ID_MISUZU},
        {"MIZUKA_", CHAR_ID_MIZUKA},
        {"NAGAMORI_", CHAR_ID_NAGAMORI},
        {"NANASE_", CHAR_ID_NANASE},
        {"EXNANASE_", CHAR_ID_EXNANASE},
        {"NAYUKI_", CHAR_ID_NAYUKI},
        {"NAYUKIB_", CHAR_ID_NAYUKIB},
        {"SHIORI_", CHAR_ID_SHIORI},
        {"AYU_", CHAR_ID_AYU},
        {"MAI_", CHAR_ID_MAI},
        {"MAYU_", CHAR_ID_MAYU},
        {"MIZUKAB_", CHAR_ID_MIZUKAB},
        {"KANO_", CHAR_ID_KANO}
    };
    
    // Convert to uppercase for case-insensitive comparison
    std::string upperName = rawName;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), 
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    
    // First try exact match
    auto it = nameToIdMap.find(upperName);
    int result = -1;
    
    if (it != nameToIdMap.end()) {
        result = it->second;
    } else {
        // If no exact match, try to find if the name starts with any of our keys
        for (const auto& pair : nameToIdMap) {
            if (upperName.find(pair.first) == 0) {
                result = pair.second;
                break;
            }
        }
    }
    
    // Only log when the result changes
    if (rawName != lastRawName || result != lastResult) {
        if (result >= 0) {
            Logger::Info("Character detected: '" + rawName + "' -> ID " + std::to_string(result) +
                        " (" + GetCharacterNameFromID(result) + ")");
        } else {
            Logger::Warning("Unknown character name: '" + rawName + "'");
        }
        
        // Update saved state
        lastRawName = rawName;
        lastResult = result;
    }
    
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
        case CHAR_ID_MIZUKA: return "Mizuka";
        case CHAR_ID_NAGAMORI: return "Nagamori";
        case CHAR_ID_NANASE: return "Nanase";
        case CHAR_ID_EXNANASE: return "Ex Nanase";
        case CHAR_ID_NAYUKI: return "Nayuki";
        case CHAR_ID_NAYUKIB: return "Nayuki B";
        case CHAR_ID_SHIORI: return "Shiori";
        case CHAR_ID_AYU: return "Ayu";
        case CHAR_ID_MAI: return "Mai";
        case CHAR_ID_MAYU: return "Mayu";
        case CHAR_ID_MIZUKAB: return "Mizuka B";
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