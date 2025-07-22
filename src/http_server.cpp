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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>

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
        
        // Fixed accept() call with proper types
        SOCKET clientSocket = accept(
            listenSocket, 
            reinterpret_cast<sockaddr*>(&clientAddr), 
            &clientAddrLen
        );
        
        if (clientSocket != INVALID_SOCKET) {
            // Get client IP for logging
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
            
            Logger::Debug("Accepted connection from " + std::string(clientIP) + 
                          ":" + std::to_string(ntohs(clientAddr.sin_port)));
            
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

void HttpServer::HandleRequest(SOCKET clientSocket) {
    LOG_FUNCTION_ENTRY();
    char buffer[2048] = {0}; // Larger buffer for detailed requests
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        // Null-terminate the received data
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        // Extract request method and path for detailed logging
        std::string method, path;
        size_t methodEnd = request.find(' ');
        if (methodEnd != std::string::npos) {
            method = request.substr(0, methodEnd);
            size_t pathEnd = request.find(' ', methodEnd + 1);
            if (pathEnd != std::string::npos) {
                path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
            }
        }
        
        Logger::Debug("HTTP Request: " + method + " " + path);
        
        // Simple routing based on path
        std::string contentType = "application/json";
        std::string jsonData = GameDataManager::GetJSONData();
        
        // Handle different paths
        if (path == "/health") {
            jsonData = "{\"status\":\"ok\"}";
        } else if (path == "/stats") {
            // Add server stats to response
            jsonData = "{\"uptime\":\"" + std::to_string(GetTickCount() / 1000) + " seconds\"}";
        }
        
        // Generate and send response
        std::string response = GenerateHttpResponse(jsonData, contentType);
        int bytesSent = send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
        
        if (bytesSent == SOCKET_ERROR) {
            Logger::Warning("Failed to send response. Error: " + std::to_string(WSAGetLastError()));
        } else {
            Logger::Debug("Sent " + std::to_string(bytesSent) + " bytes response");
        }
    } else if (bytesReceived == 0) {
        Logger::Debug("Client closed connection");
    } else {
        Logger::Warning("Failed to receive data. Error: " + std::to_string(WSAGetLastError()));
    }
    
    LOG_FUNCTION_EXIT();
}

std::string HttpServer::GenerateHttpResponse(const std::string& content, const std::string& contentType) {
    LOG_FUNCTION_ENTRY();
    
    std::ostringstream response;
    
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: EFZ-Streaming-Overlay/1.0\r\n";  // Add server identifier
    response << "\r\n";
    response << content;
    
    Logger::Debug("Generated HTTP response (" + std::to_string(response.str().length()) + " bytes)");
    LOG_FUNCTION_EXIT();
    return response.str();
}

#pragma warning(pop)