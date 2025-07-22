#include "../include/memory_reader.h"
#include "../include/constants.h"
#include "../include/logger.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <cstring>
#include <thread>

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
    #ifdef VERBOSE_LOGGING
    Logger::Debug("Reading " + std::to_string(size) + " bytes from " + Logger::FormatHex(address));
    #endif
    
    SIZE_T bytesRead;
    bool result = ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, &bytesRead) && bytesRead == size;
    
    if (!result) {
        LOG_WIN32_ERROR("ReadProcessMemory failed at address " + Logger::FormatHex(address));
    }
    
    Logger::LogMemoryOperation(address, "Read", result, size);
    return result;
}

DWORD MemoryReader::ReadDWORD(DWORD address) {
    DWORD value = 0;
    if (ReadMemory(address, &value, sizeof(DWORD))) {
        Logger::Debug("ReadDWORD at " + Logger::FormatHex(address) + " = " + Logger::FormatHex(value));
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

int MemoryReader::ReadByte(DWORD address) {
    BYTE value = 0;
    if (ReadMemory(address, &value, sizeof(BYTE))) {
        Logger::Debug("ReadByte at " + Logger::FormatHex(address) + " = " + std::to_string((int)value));
    }
    return (int)value;
}

DWORD MemoryReader::GetP1WinCount() {
    LOG_FUNCTION_ENTRY();
    DWORD result = 0;
    
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P1_WIN_COUNT_OFFSET;
            result = ReadDWORD(finalAddr);
            Logger::Debug("Revival P1 Win Count: " + std::to_string(result));
        }
    }
    
    LOG_FUNCTION_EXIT();
    return result;
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

std::wstring MemoryReader::GetP1Nickname() {
    // Try again to find the module if not found yet
    TryLoadEfzRevivalModule();
    
    std::wstring result;
    if (efzRevivalModule) {
        // Revival mod uses a different memory structure
        DWORD baseAddr = (DWORD)efzRevivalModule + WIN_COUNT_BASE_OFFSET;
        DWORD ptrValue = ReadDWORD(baseAddr);
        if (ptrValue != 0) {
            DWORD finalAddr = ptrValue + P1_NICKNAME_OFFSET;
            result = ReadWideString(finalAddr, MAX_NICKNAME_LENGTH);
        }
    }
    
    // If empty or failed to read, use a default
    if (result.empty()) {
        result = L"Player 1";
    }
    
    return result;
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

int MemoryReader::GetP1CharacterID() {
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P1;
    DWORD p1Addr = ReadDWORD(baseAddr);
    if (p1Addr == 0) return -1;
    return ReadByte(p1Addr + CHARACTER_NAME_OFFSET);
}

int MemoryReader::GetP2CharacterID() {
    DWORD baseAddr = (DWORD)efzModule + EFZ_BASE_OFFSET_P2;
    DWORD p2Addr = ReadDWORD(baseAddr);
    if (p2Addr == 0) return -1;
    return ReadByte(p2Addr + CHARACTER_NAME_OFFSET);
}

std::string MemoryReader::GetP1CharacterName() {
    int charId = GetP1CharacterID();
    if (charId >= 0 && charId <= MAX_CHARACTER_ID) {
        return std::string(CHARACTER_NAMES[charId]);
    }
    return "Undefined";
}

std::string MemoryReader::GetP2CharacterName() {
    int charId = GetP2CharacterID();
    if (charId >= 0 && charId <= MAX_CHARACTER_ID) {
        return std::string(CHARACTER_NAMES[charId]);
    }
    return "Undefined";
}
