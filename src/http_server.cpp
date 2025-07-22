#pragma warning(push)
#pragma warning(disable: 4005)  // Disable warning about macro redefinition

// Define these before ANY other includes to prevent winsock.h from being included
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include "../include/http_server.h"
#include "../include/game_data.h"
#include "../include/logger.h"
#include "../include/overlay_data.h"  // Add this include
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")

HANDLE HttpServer::serverThread = nullptr;
bool HttpServer::running = false;
int HttpServer::serverPort = 8080;

bool HttpServer::Initialize(int port) {
    LOG_FUNCTION_ENTRY();
    Logger::Info("Initializing HTTP server on port " + std::to_string(port));
    
    serverPort = port;
    
    // Initialize Winsock with detailed error handling
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Logger::Error("WSAStartup failed with error code: " + std::to_string(result));
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    // Verify Winsock version
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        Logger::Error("Required Winsock version not available");
        WSACleanup();
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    Logger::Info("Winsock initialized successfully (version " + 
                 std::to_string(LOBYTE(wsaData.wVersion)) + "." +
                 std::to_string(HIBYTE(wsaData.wVersion)) + ")");
    
    running = true;
    serverThread = CreateThread(nullptr, 0, ServerThreadProc, nullptr, 0, nullptr);
    
    if (serverThread == nullptr) {
        LOG_WIN32_ERROR("Failed to create server thread");
        WSACleanup();
        running = false;
        LOG_FUNCTION_EXIT();
        return false;
    }
    
    Logger::Info("HTTP server started successfully");
    LOG_FUNCTION_EXIT();
    return true;
}

void HttpServer::Shutdown() {
    LOG_FUNCTION_ENTRY();
    Logger::Info("Shutting down HTTP server");
    
    running = false;
    
    if (serverThread) {
        Logger::Debug("Waiting for server thread to end");
        // Give the thread time to clean up
        DWORD result = WaitForSingleObject(serverThread, 5000);
        
        if (result == WAIT_TIMEOUT) {
            Logger::Warning("HTTP server thread did not terminate within timeout - forcing termination");
            TerminateThread(serverThread, 0);
        } else if (result == WAIT_FAILED) {
            LOG_WIN32_ERROR("WaitForSingleObject failed");
        } else {
            Logger::Debug("Server thread ended gracefully");
        }
        
        CloseHandle(serverThread);
        serverThread = nullptr;
    }
    
    WSACleanup();
    Logger::Info("HTTP server shutdown complete");
    LOG_FUNCTION_EXIT();
}

bool HttpServer::IsRunning() {
    return running;
}

DWORD WINAPI HttpServer::ServerThreadProc(LPVOID lpParam) {
    // Keep entry log
    LOG_FUNCTION_ENTRY();
    SOCKET listenSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    
    // Create socket with error handling
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        Logger::Error("Failed to create listen socket. Error: " + std::to_string(WSAGetLastError()));
        LOG_FUNCTION_EXIT();
        return 1;
    }
    
    Logger::Debug("Socket created successfully");
    
    // Setup server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(static_cast<u_short>(serverPort));
    
    // Enable address reuse for faster restart
    int optval = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) != 0) {
        Logger::Warning("Failed to set SO_REUSEADDR option. Error: " + std::to_string(WSAGetLastError()));
    }
    
    // Bind socket with detailed error handling
    if (bind(listenSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        Logger::Error("Bind failed. Error: " + std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        LOG_FUNCTION_EXIT();
        return 1;
    }
    
    Logger::Debug("Socket bound to port " + std::to_string(serverPort));
    
    // Start listening with detailed error handling
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::Error("Listen failed. Error: " + std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        LOG_FUNCTION_EXIT();
        return 1;
    }
    
    Logger::Info("Server listening on port " + std::to_string(serverPort));
    
    // Set socket to non-blocking mode for graceful shutdown
    u_long mode = 1;
    if (ioctlsocket(listenSocket, FIONBIO, &mode) != 0) {
        Logger::Warning("Failed to set non-blocking mode. Error: " + std::to_string(WSAGetLastError()));
    }
    
    // Main server loop with robust error handling
    while (running) {
        // Accept incoming connections
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(
            listenSocket, 
            reinterpret_cast<sockaddr*>(&clientAddr), 
            &clientAddrLen
        );
        
        if (clientSocket != INVALID_SOCKET) {
            // Log connection less frequently
            static DWORD lastConnectionLogTime = 0;
            static int connectionsSinceLastLog = 0;
            DWORD currentTime = GetTickCount();
            
            connectionsSinceLastLog++;
            if (currentTime - lastConnectionLogTime > 10000) { // Every 10 seconds
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
                
                Logger::Debug("Handled " + std::to_string(connectionsSinceLastLog) + 
                             " connections in last 10s (latest from " + 
                             std::string(clientIP) + ":" + 
                             std::to_string(ntohs(clientAddr.sin_port)) + ")");
                
                connectionsSinceLastLog = 0;
                lastConnectionLogTime = currentTime;
            }
            
            // Set timeout for client operations
            DWORD timeout = 2000; // 2 seconds
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            
            HandleRequest(clientSocket);
            closesocket(clientSocket);
        } else {
            // Check if error is just "would block" (expected for non-blocking socket)
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                Logger::Warning("Accept failed. Error: " + std::to_string(error));
            }
            
            // No connection available, sleep briefly to avoid high CPU usage
            Sleep(10);
        }
    }
    
    // Cleanup
    Logger::Debug("Closing listen socket");
    closesocket(listenSocket);
    Logger::Info("Server thread ended");
    LOG_FUNCTION_EXIT();
    return 0;
}

// Add this function to determine content type based on file extension
std::string GetContentType(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    static const std::map<std::string, std::string> contentTypes = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"}
    };
    
    auto it = contentTypes.find(ext);
    if (it != contentTypes.end()) {
        return it->second;
    }
    
    return "application/octet-stream"; // Default binary type
}

// Add this function to read files
std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// Update the HandleRequest function to throttle logging
void HttpServer::HandleRequest(SOCKET clientSocket) {
    // Track these across function calls to throttle request logging
    static std::unordered_map<std::string, DWORD> lastPathLogTime;
    static const int PATH_LOG_THROTTLE_MS = 5000; // Only log same path every 5 seconds
    
    char buffer[2048] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        std::string method, path;
        size_t methodEnd = request.find(' ');
        if (methodEnd != std::string::npos) {
            method = request.substr(0, methodEnd);
            size_t pathEnd = request.find(' ', methodEnd + 1);
            if (pathEnd != std::string::npos) {
                path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
            }
        }
        
        // Throttle HTTP request logging - only log unique paths or after timeout
        DWORD currentTime = GetTickCount();
        bool shouldLog = false;
        
        auto it = lastPathLogTime.find(path);
        if (it == lastPathLogTime.end() || (currentTime - it->second) > PATH_LOG_THROTTLE_MS) {
            lastPathLogTime[path] = currentTime;
            shouldLog = true;
        }
        
        if (shouldLog) {
            Logger::Debug("HTTP Request: " + method + " " + path);
        }
        
        // Default path handling
        if (path == "/" || path == "/index.html") {
            path = "/overlay.html";
        }
        
        // Handle API endpoints
        if (path == "/api/data" || path == "/api" || path == "/data") {
            std::string jsonData = GameDataManager::GetJSONData();
            std::string response = GenerateHttpResponse(jsonData, "application/json");
            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
            return;
        }
        else if (path == "/health") {
            std::string jsonData = "{\"status\":\"ok\"}";
            std::string response = GenerateHttpResponse(jsonData, "application/json");
            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
            return;
        }
        
        // Handle file requests
        // Get the overlay assets directory from OverlayData
        std::string assetsPath = OverlayData::GetOutputDirectory();
        std::string filePath = assetsPath + path;
        std::string fileContent;
        
        // Make the path safe - prevent directory traversal attacks
        std::filesystem::path normalizedPath = std::filesystem::path(filePath).lexically_normal();
        std::filesystem::path assetsDir = std::filesystem::path(assetsPath);
        
        // Check if the path is within the assets directory
        if (normalizedPath.string().find(assetsDir.string()) == 0 && 
            std::filesystem::exists(normalizedPath)) {
            
            fileContent = ReadFile(normalizedPath.string());
            if (!fileContent.empty()) {
                std::string contentType = GetContentType(path);
                std::string response = GenerateHttpResponse(fileContent, contentType);
                send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                
                // Only log file served if we're logging this request
                if (shouldLog) {
                    Logger::Debug("Served file: " + normalizedPath.string());
                }
                return;
            }
        }
        
        // If we get here, the file wasn't found or couldn't be read
        std::string notFoundResponse = "HTTP/1.1 404 Not Found\r\n";
        notFoundResponse += "Content-Type: text/plain\r\n";
        notFoundResponse += "Content-Length: 13\r\n\r\n";
        notFoundResponse += "404 Not Found";
        
        send(clientSocket, notFoundResponse.c_str(), static_cast<int>(notFoundResponse.length()), 0);
        
        // Always log file not found errors (important for debugging)
        Logger::Warning("File not found: " + path);
    }
}

// Also modify the GenerateHttpResponse to reduce logging
std::string HttpServer::GenerateHttpResponse(const std::string& content, const std::string& contentType) {
    // No entry/exit logging to reduce console spam
    
    std::ostringstream response;
    
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response << "Access-Control-Allow-Methods: GET\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: EFZ-Streaming-Overlay/1.0\r\n";
    response << "\r\n";
    response << content;
    
    // Only log the size occasionally using the throttled version
    static DWORD lastLogTime = 0;
    DWORD currentTime = GetTickCount();
    if (currentTime - lastLogTime > 10000) { // Once every 10 seconds
        Logger::Debug("Generated HTTP response (" + std::to_string(response.str().length()) + " bytes)");
        lastLogTime = currentTime;
    }
    
    return response.str();
}

// Update the HTTP response headers to include CORS
std::string HttpServer::BuildHttpResponse(const std::string& jsonData) {
    std::string response;
    
    // HTTP headers with CORS
    response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Access-Control-Allow-Origin: *\r\n"; // Allow CORS from any origin
    response += "Access-Control-Allow-Methods: GET\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Content-Length: " + std::to_string(jsonData.length()) + "\r\n\r\n";
    response += jsonData;
    
    return response;
}

#pragma warning(pop)