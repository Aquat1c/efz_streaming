#pragma once
// Add Windows.h include with proper defines to prevent conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#include <string>
#include <fstream>
#include <vector>

class Logger {
public:
    enum Level {
        LOG_DEBUG,    // Most detailed information
        LOG_INFO,     // General information
        LOG_WARNING,  // Warnings
        LOG_ERROR,    // Errors
        LOG_CRITICAL  // Critical errors
    };
    
    static void Initialize(const std::string& filename = "efz_streaming.log", Level minLevel = LOG_DEBUG, bool enableConsole = true);
    static void Log(Level level, const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Debug(const std::string& message);
    static void Critical(const std::string& message);
    static void Shutdown();
    
    // Helper for memory operations logging
    static void LogMemoryOperation(DWORD address, const std::string& operation, bool success, int size = 0);
    
    // Helper for hex formatting
    static std::string FormatHex(DWORD value);
    static void LogSystemInfo();
    
    // Console management
    static bool CreateConsole();
    static void CloseConsole();
    static bool HasConsole() { return consoleAttached; }
    
    static void EnableMemoryOperationFiltering(bool enable);
    static bool IsMemoryOperationFilteringEnabled();
    
private:
    static std::ofstream logFile;
    static bool initialized;
    static Level minimumLevel;
    static std::string logFilePath;
    static std::vector<std::string> pendingMessages;
    static bool consoleAttached;
    static HWND consoleWindow;
    
    static bool filterMemoryOperations;
    static DWORD lastLogTick;
    static int memoryOperationsCount;
    static const int MEMORY_LOGS_REPORT_THRESHOLD;
    static const int LOG_SUMMARY_INTERVAL_MS;
    
    static std::string GetTimestamp();
    static std::string LevelToString(Level level);
};

#define LOG_FUNCTION_ENTRY() Logger::Debug(std::string(__FUNCTION__) + " - Entry")
#define LOG_FUNCTION_EXIT() Logger::Debug(std::string(__FUNCTION__) + " - Exit")

#define LOG_WIN32_ERROR(msg) { \
    DWORD error = GetLastError(); \
    std::string errorMsg = msg; \
    errorMsg += " - Error code: " + std::to_string(error); \
    char errorBuf[256]; \
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, errorBuf, sizeof(errorBuf), NULL); \
    errorMsg += " (" + std::string(errorBuf) + ")"; \
    Logger::Error(errorMsg); \
}
