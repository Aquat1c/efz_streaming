#include "../include/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <windows.h>
#include <algorithm>

std::ofstream Logger::logFile;
bool Logger::initialized = false;
Logger::Level Logger::minimumLevel = Logger::LOG_DEBUG;
std::string Logger::logFilePath;
std::vector<std::string> Logger::pendingMessages;
bool Logger::consoleAttached = false;
HWND Logger::consoleWindow = nullptr;

// Fix: Remove 'static' keyword from definitions (keep it in declarations only)
bool Logger::filterMemoryOperations = true;
DWORD Logger::lastLogTick = 0;
int Logger::memoryOperationsCount = 0;
const int Logger::MEMORY_LOGS_REPORT_THRESHOLD = 100;
const int Logger::LOG_SUMMARY_INTERVAL_MS = 5000;

void Logger::Initialize(const std::string& filename, Level minLevel, bool enableConsole) {
    minimumLevel = minLevel;
    logFilePath = filename;
    
    // Create console window if requested
    if (enableConsole) {
        CreateConsole();
    }
    
    // Open log file with timestamp in name for debugging purposes
    try {
        logFile.open(filename, std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            initialized = true;
            
            // Print any messages that were logged before initialization
            for (const auto& msg : pendingMessages) {
                logFile << msg << std::endl;
                if (consoleAttached) {
                    std::cout << msg << std::endl;
                }
            }
            pendingMessages.clear();
            
            Info("-------------- Logger initialized --------------");
            LogSystemInfo();
        }
        else {
            if (consoleAttached) {
                std::cerr << "Failed to open log file: " << filename << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        if (consoleAttached) {
            std::cerr << "Exception in logger initialization: " << e.what() << std::endl;
        }
    }
}

bool Logger::CreateConsole() {
    if (consoleAttached) {
        return true; // Already created
    }
    
    // Try to create a new console
    if (AllocConsole()) {
        // Redirect standard I/O to the new console
        FILE* pFile = nullptr;
        freopen_s(&pFile, "CONOUT$", "w", stdout);
        freopen_s(&pFile, "CONOUT$", "w", stderr);
        freopen_s(&pFile, "CONIN$", "r", stdin);
        
        // Set console properties
        consoleWindow = GetConsoleWindow();
        if (consoleWindow) {
            // Set console title
            SetConsoleTitleA("EFZ Streaming Overlay - Debug Console");
            
            // Position console window (top-right corner)
            RECT desktop;
            GetWindowRect(GetDesktopWindow(), &desktop);
            
            RECT console;
            GetWindowRect(consoleWindow, &console);
            int width = console.right - console.left;
            int height = console.bottom - console.top;
            
            // Move to top-right of screen
            MoveWindow(consoleWindow, 
                       desktop.right - width, // X position (right side of screen)
                       0,                    // Y position (top of screen)
                       width,                // Width
                       height,               // Height
                       TRUE);
            
            // Set console buffer size for better scrolling
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                COORD bufferSize;
                bufferSize.X = csbi.dwSize.X;
                bufferSize.Y = 9999; // Large scroll buffer
                SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), bufferSize);
            }
        }
        
        consoleAttached = true;
        std::cout << "EFZ Streaming Overlay - Debug Console" << std::endl;
        std::cout << "==================================" << std::endl;
        return true;
    }
    return false;
}

void Logger::CloseConsole() {
    if (!consoleAttached) {
        return;
    }
    
    // Detach standard I/O streams
    FILE* pFile = nullptr;
    freopen_s(&pFile, "NUL:", "w", stdout);
    freopen_s(&pFile, "NUL:", "w", stderr);
    freopen_s(&pFile, "NUL:", "r", stdin);
    
    // Free the console
    FreeConsole();
    consoleAttached = false;
    consoleWindow = nullptr;
}

void Logger::LogSystemInfo() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    OSVERSIONINFOEX osInfo;
    ZeroMemory(&osInfo, sizeof(OSVERSIONINFOEX));
    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    #pragma warning(disable:4996)
    GetVersionEx((OSVERSIONINFO*)&osInfo);
    #pragma warning(default:4996)
    
    std::ostringstream oss;
    oss << "System Information:" << std::endl;
    oss << "- Windows Version: " << osInfo.dwMajorVersion << "." << osInfo.dwMinorVersion
        << " (Build " << osInfo.dwBuildNumber << ")" << std::endl;
    
    oss << "- Processor Architecture: ";
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: oss << "x64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: oss << "x86"; break;
        default: oss << "Unknown"; break;
    }
    
    oss << std::endl;
    oss << "- Number of Processors: " << sysInfo.dwNumberOfProcessors << std::endl;
    
    // Get current module file path
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    oss << "- Module Path: " << modulePath << std::endl;
    
    // Get process info
    oss << "- Process ID: " << GetCurrentProcessId() << std::endl;
    
    Info(oss.str());
}

void Logger::Log(Level level, const std::string& message) {
    if (level < minimumLevel) return;
    
    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string threadId = std::to_string(GetCurrentThreadId());
    
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] [Thread:" + threadId + "] " + message;
    
    if (!initialized) {
        // Store message to be written when logger is initialized
        pendingMessages.push_back(logMessage);
        if (consoleAttached) {
            std::cout << logMessage << std::endl;
        }
        return;
    }
    
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.flush();
    }
    
    // Also output to console for debugging
    if (consoleAttached) {
        // Set color based on log level
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        WORD defaultAttribs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        WORD attribs = defaultAttribs;
        
        switch (level) {
            case LOG_DEBUG:
                attribs = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break;
            case LOG_INFO:
                attribs = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                break;
            case LOG_WARNING:
                attribs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break;
            case LOG_ERROR:
                attribs = FOREGROUND_RED | FOREGROUND_INTENSITY;
                break;
            case LOG_CRITICAL:
                attribs = FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_INTENSITY;
                break;
        }
        
        SetConsoleTextAttribute(hConsole, attribs);
        std::cout << logMessage << std::endl;
        SetConsoleTextAttribute(hConsole, defaultAttribs);
    }
}

void Logger::Info(const std::string& message) {
    Log(LOG_INFO, message);
}

void Logger::Warning(const std::string& message) {
    Log(LOG_WARNING, message);
}

void Logger::Error(const std::string& message) {
    Log(LOG_ERROR, message);
}

void Logger::Debug(const std::string& message) {
    #ifdef VERBOSE_LOGGING
    Log(LOG_DEBUG, message);
    #endif
}

void Logger::Critical(const std::string& message) {
    Log(LOG_CRITICAL, message);
    
    // For critical errors, also show a message box in debug builds
    #ifdef _DEBUG
    MessageBoxA(NULL, message.c_str(), "Critical Error", MB_ICONERROR | MB_OK);
    #endif
}

// Modify the LogMemoryOperation function to filter repeated messages
void Logger::LogMemoryOperation(DWORD address, const std::string& operation, bool success, int size) {
    #ifdef VERBOSE_LOGGING
    if (filterMemoryOperations) {
        // If filtering is enabled, count operations and report summaries
        memoryOperationsCount++;
        
        DWORD currentTick = GetTickCount();
        if (!success || (currentTick - lastLogTick > LOG_SUMMARY_INTERVAL_MS && memoryOperationsCount > 0)) {
            // Report summary every 5 seconds or on error
            std::ostringstream oss;
            oss << "Memory operations in last " << (currentTick - lastLogTick) / 1000 
                << " seconds: " << memoryOperationsCount << " (" 
                << (memoryOperationsCount / (std::max)(1, static_cast<int>((currentTick - lastLogTick) / 1000))) 
                << "/sec)";
            Debug(oss.str());
            
            // Reset counter and timer
            memoryOperationsCount = 0;
            lastLogTick = currentTick;
        }
        
        // Always log errors
        if (!success) {
            std::ostringstream oss;
            oss << operation << " at " << FormatHex(address);
            if (size > 0) {
                oss << " (size: " << size << " bytes)";
            }
            DWORD error = GetLastError();
            oss << " - Failed (Error: " << error << ")";
            Error(oss.str());
        }
    } else {
        // Traditional verbose logging (every operation)
        std::ostringstream oss;
        oss << operation << " at " << FormatHex(address);
        
        if (size > 0) {
            oss << " (size: " << size << " bytes)";
        }
        
        if (success) {
            oss << " - Success";
        } else {
            DWORD error = GetLastError();
            oss << " - Failed (Error: " << error << ")";
        }
        
        Debug(oss.str());
    }
    #endif
}

// Add these functions to toggle filtering
void Logger::EnableMemoryOperationFiltering(bool enable) {
    filterMemoryOperations = enable;
    lastLogTick = GetTickCount();
    memoryOperationsCount = 0;
    Info(std::string("Memory operation filtering ") + 
        (enable ? "enabled" : "disabled"));
}

bool Logger::IsMemoryOperationFilteringEnabled() {
    return filterMemoryOperations;
}

std::string Logger::FormatHex(DWORD value) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << value;
    return oss.str();
}

void Logger::Shutdown() {
    if (initialized && logFile.is_open()) {
        Info("-------------- Logger shutting down --------------");
        logFile.close();
        initialized = false;
    }
    
    // Close the console if we created it
    if (consoleAttached) {
        CloseConsole();
    }
}

std::string Logger::GetTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::LevelToString(Level level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO ";
        case LOG_WARNING: return "WARN ";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRIT ";
        default: return "UNKWN";
    }
}
