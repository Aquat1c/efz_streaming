#pragma once
// Define these before including windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <string>
#include <windows.h>

struct PlayerData {
    std::string nickname;
    std::string character;
    int characterId;
    int winCount;
};

struct GameData {
    PlayerData player1;
    PlayerData player2;
    bool gameActive;
    
    std::string ToJSON() const;
};

class GameDataManager {
public:
    static bool Initialize();
    static bool Update(); // Change return type to bool
    static const GameData& GetCurrentData();
    static std::string GetJSONData();
    static void Shutdown();
    
private:
    static GameData currentData;
    static bool initialized;
    static bool running;
    static HANDLE updateThread;
    static DWORD WINAPI UpdateThreadProc(LPVOID lpParam);

    // Add these members for change detection
    static GameData previousData;
    static bool HasDataChanged();
    static void LogChanges();
};
