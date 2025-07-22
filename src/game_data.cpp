#include "../include/game_data.h"
#include "../include/memory_reader.h"
#include "../include/logger.h"
#include "../include/overlay_data.h"
#include <codecvt>
#include <locale>

GameData GameDataManager::currentData = {};
GameData GameDataManager::previousData = {}; // Add this line
bool GameDataManager::initialized = false;
bool GameDataManager::running = false;
HANDLE GameDataManager::updateThread = nullptr;

bool GameDataManager::Initialize() {
    Logger::Info("Initializing game data manager");
    initialized = true;
    running = true;
    
    // Start the background update thread
    updateThread = CreateThread(nullptr, 0, UpdateThreadProc, nullptr, 0, nullptr);
    if (updateThread == nullptr) {
        Logger::Error("Failed to create update thread");
        return false;
    }
    
    return true;
}

bool GameDataManager::HasDataChanged() {
    // Check for significant changes only
    return (currentData.player1.characterId != previousData.player1.characterId ||
            currentData.player2.characterId != previousData.player2.characterId ||
            currentData.player1.winCount != previousData.player1.winCount ||
            currentData.player2.winCount != previousData.player2.winCount ||
            currentData.gameActive != previousData.gameActive);
}

void GameDataManager::LogChanges() {
    // Only log the specific changes that occurred
    if (currentData.player1.characterId != previousData.player1.characterId) {
        Logger::Info("P1 character changed: " + 
            (previousData.player1.character.empty() ? "None" : previousData.player1.character) + 
            " -> " + currentData.player1.character);
    }
    
    if (currentData.player2.characterId != previousData.player2.characterId) {
        Logger::Info("P2 character changed: " + 
            (previousData.player2.character.empty() ? "None" : previousData.player2.character) + 
            " -> " + currentData.player2.character);
    }
    
    if (currentData.player1.winCount != previousData.player1.winCount) {
        Logger::Info("P1 wins changed: " + std::to_string(previousData.player1.winCount) + 
                    " -> " + std::to_string(currentData.player1.winCount));
    }
    
    if (currentData.player2.winCount != previousData.player2.winCount) {
        Logger::Info("P2 wins changed: " + std::to_string(previousData.player2.winCount) + 
                    " -> " + std::to_string(currentData.player2.winCount));
    }
    
    if (currentData.gameActive != previousData.gameActive) {
        Logger::Info("Game state changed: " + 
                    std::string(previousData.gameActive ? "active" : "inactive") + 
                    " -> " + std::string(currentData.gameActive ? "active" : "inactive"));
    }
    
    // Update previous data for next comparison
    previousData = currentData;
}

bool GameDataManager::Update() {
    if (!initialized) return false;
    
    // Store old values for comparison
    GameData oldData = currentData;
    
    // Convert wide strings to UTF-8
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    
    try {
        // Update Player 1 data
        std::wstring p1Nick = MemoryReader::GetP1Nickname();
        currentData.player1.nickname = converter.to_bytes(p1Nick);
        
        // Get character data with proper error handling
        std::string p1CharRaw = MemoryReader::GetP1CharacterNameRaw();
        if (!p1CharRaw.empty()) {
            currentData.player1.characterId = MemoryReader::GetP1CharacterID();
            currentData.player1.character = MemoryReader::GetP1CharacterName();
        } else {
            // No character data available
            currentData.player1.characterId = -1;
            currentData.player1.character = "Unknown";
        }
        
        currentData.player1.winCount = MemoryReader::GetP1WinCount();
        
        // Update Player 2 data with similar protection
        std::wstring p2Nick = MemoryReader::GetP2Nickname();
        currentData.player2.nickname = converter.to_bytes(p2Nick);
        
        std::string p2CharRaw = MemoryReader::GetP2CharacterNameRaw();
        if (!p2CharRaw.empty()) {
            currentData.player2.characterId = MemoryReader::GetP2CharacterID();
            currentData.player2.character = MemoryReader::GetP2CharacterName();
        } else {
            currentData.player2.characterId = -1;
            currentData.player2.character = "Unknown";
        }
        
        currentData.player2.winCount = MemoryReader::GetP2WinCount();
        
        // Check if game is active based on valid character IDs
        currentData.gameActive = (currentData.player1.characterId >= 0 && 
                                currentData.player2.characterId >= 0);
        
        // Log only when important data changes
        bool changed = HasDataChanged();
        if (changed) {
            LogChanges();
        }
        
        return changed;
    } catch (const std::exception& e) {
        Logger::Error("Error updating game data: " + std::string(e.what()));
        return false;
    }
}

const GameData& GameDataManager::GetCurrentData() {
    return currentData;
}

std::string GameDataManager::GetJSONData() {
    return currentData.ToJSON(); // Don't update here since background thread is doing it
}

void GameDataManager::Shutdown() {
    Logger::Info("Shutting down game data manager");
    running = false;
    
    if (updateThread) {
        WaitForSingleObject(updateThread, 5000);
        CloseHandle(updateThread);
        updateThread = nullptr;
    }
}

// Modify UpdateThreadProc to run less frequently
DWORD WINAPI GameDataManager::UpdateThreadProc(LPVOID lpParam) {
    Logger::Info("Game data update thread started");
    
    while (running) {
        // Only update files when data actually changes
        if (Update()) {
            OverlayData::UpdateFiles();
        }
        Sleep(100); // Update every 100ms (10 times per second)
    }
    
    Logger::Info("Game data update thread ended");
    return 0;
}

std::string GameData::ToJSON() const {
    std::string json = "{\n";
    json += "  \"player1\": {\n";
    json += "    \"nickname\": \"" + player1.nickname + "\",\n";
    json += "    \"character\": \"" + player1.character + "\",\n";
    json += "    \"characterId\": " + std::to_string(player1.characterId) + ",\n";
    json += "    \"winCount\": " + std::to_string(player1.winCount) + "\n";
    json += "  },\n";
    json += "  \"player2\": {\n";
    json += "    \"nickname\": \"" + player2.nickname + "\",\n";
    json += "    \"character\": \"" + player2.character + "\",\n";
    json += "    \"characterId\": " + std::to_string(player2.characterId) + ",\n";
    json += "    \"winCount\": " + std::to_string(player2.winCount) + "\n";
    json += "  },\n";
    json += "  \"gameActive\": " + std::string(gameActive ? "true" : "false") + "\n";
    json += "}";
    return json;
}
