#pragma once
// Define these before ANY other includes
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <string>

// Forward declaration for SOCKET type
typedef UINT_PTR SOCKET;

class HttpServer {
public:
    static bool Initialize(int port = 8080);
    static void Shutdown();
    static bool IsRunning();
    
private:
    static HANDLE serverThread;
    static bool running;
    static int serverPort;
    
    static DWORD WINAPI ServerThreadProc(LPVOID lpParam);
    static void HandleRequest(SOCKET clientSocket);
    static std::string GenerateHttpResponse(const std::string& content, const std::string& contentType = "application/json");
};