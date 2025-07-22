#include <windows.h>
#include "../include/memory_reader.h"
#include "../include/game_data.h"
#include "../include/http_server.h"
#include "../include/overlay_data.h"
#include "../include/logger.h"
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

// Forward declaration for ConsoleCommandThread
DWORD WINAPI ConsoleCommandThread(LPVOID lpParam);

// Global flag for DLL initialization status
bool g_dllInitialized = false;
bool g_consoleCommandThreadRunning = false;
HANDLE g_consoleCommandThread = nullptr;

// Function to initialize all components
bool InitializeComponents() {
    // Initialize logging first with console enabled
    Logger::Initialize("efz_streaming.log", Logger::LOG_DEBUG, true);
    Logger::Info("======== EFZ Streaming Overlay DLL Loading ========");
    Logger::Info("Module base address: " + Logger::FormatHex((DWORD)GetModuleHandleA(NULL)));
    
    // Enable filtering by default to reduce spam
    Logger::EnableMemoryOperationFiltering(true);
    
    // Start console command thread
    if (Logger::HasConsole()) {
        g_consoleCommandThreadRunning = true;
        g_consoleCommandThread = CreateThread(nullptr, 0, ConsoleCommandThread, nullptr, 0, nullptr);
        if (!g_consoleCommandThread) {
            Logger::Warning("Failed to start console command thread");
        }
    }
    
    // Initialize components with error handling
    if (!MemoryReader::Initialize()) {
        Logger::Error("Failed to initialize memory reader - aborting");
        return false;
    }
    
    // Continue with initialization even if EfzRevival.dll isn't available yet
    // It will be detected later by the background thread
    
    if (!GameDataManager::Initialize()) {
        Logger::Error("Failed to initialize game data manager - aborting");
        MemoryReader::Shutdown(); // Cleanup already initialized components
        return false;
    }
    
    if (!OverlayData::Initialize()) {
        Logger::Error("Failed to initialize overlay data - aborting");
        GameDataManager::Shutdown();
        MemoryReader::Shutdown();
        return false;
    }
    
    // Initialize HTTP server last
    if (!HttpServer::Initialize(8080)) {
        Logger::Error("Failed to initialize HTTP server - aborting");
        OverlayData::Shutdown();
        GameDataManager::Shutdown();
        MemoryReader::Shutdown();
        return false;
    }
    
    Logger::Info("All components initialized successfully");
    return true;
}

// Function to clean up all components in reverse order
void CleanupComponents() {
    Logger::Info("======== EFZ Streaming Overlay DLL Unloading ========");
    
    // Stop command thread
    if (g_consoleCommandThread != nullptr) {
        g_consoleCommandThreadRunning = false;
        
        // Give the thread time to exit
        WaitForSingleObject(g_consoleCommandThread, 1000);
        CloseHandle(g_consoleCommandThread);
        g_consoleCommandThread = nullptr;
    }
    
    // Clean up in reverse order of initialization
    HttpServer::Shutdown();
    OverlayData::Shutdown();
    GameDataManager::Shutdown();
    MemoryReader::Shutdown(); // This will also stop the module watcher thread
    
    // Shutdown logger last
    Logger::Info("All components shut down successfully");
    Logger::Shutdown();
}

DWORD WINAPI ConsoleCommandThread(LPVOID lpParam) {
    Logger::Info("Console command interface started - type 'help' for available commands");
    
    while (g_consoleCommandThreadRunning) {
        // Check if console exists
        if (!Logger::HasConsole()) {
            Sleep(1000);
            continue;
        }
        
        std::string input;
        std::cout << "\n> ";
        std::getline(std::cin, input);
        
        // Convert to lowercase for case-insensitive commands
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), 
                     [](unsigned char c){ return std::tolower(c); });
        
        if (cmd == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  help          - Show this help\n";
            std::cout << "  filter on     - Enable memory operation filtering (reduce spam)\n";
            std::cout << "  filter off    - Disable memory operation filtering (show all reads)\n";
            std::cout << "  quit          - Exit the command interface\n";
            std::cout << "  clear         - Clear the console\n";
        } 
        else if (cmd == "filter on") {
            Logger::EnableMemoryOperationFiltering(true);
        }
        else if (cmd == "filter off") {
            Logger::EnableMemoryOperationFiltering(false);
        }
        else if (cmd == "clear") {
            system("cls");
        }
        else if (cmd == "quit") {
            g_consoleCommandThreadRunning = false;
        }
        else if (!cmd.empty()) {
            std::cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }
    
    Logger::Info("Console command interface stopped");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule); // Optimize DLL performance
        
        // Initialize components
        g_dllInitialized = InitializeComponents();
        return g_dllInitialized; // Return FALSE if initialization failed
        
    case DLL_PROCESS_DETACH:
        if (g_dllInitialized) {
            // Only clean up if we successfully initialized
            CleanupComponents();
        }
        break;
        
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // These are disabled by DisableThreadLibraryCalls
        break;
    }
    return TRUE;
}
