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

// Initialize static variables
DWORD GameDataManager::lastWinCountCheck = 0;
DWORD GameDataManager::cachedP1WinCount = 0;
DWORD GameDataManager::cachedP2WinCount = 0;

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
    
    // Store previous data for change detection
    GameData prevData = currentData;
    
    try {
        // Check if we're transitioning from no characters to characters selected
        bool wasCharacterSelected = (prevData.player1.characterId >= 0 || prevData.player2.characterId >= 0);
        
        // Update current game state from memory
        // Convert wide strings to UTF-8
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        
        // Update Player 1 data
        std::wstring p1Nick = MemoryReader::GetP1Nickname();
        currentData.player1.nickname = converter.to_bytes(p1Nick);
        
        // Get character data with forced refresh when needed
        std::string p1CharRaw = MemoryReader::GetP1CharacterNameRaw();
        if (!p1CharRaw.empty()) {
            currentData.player1.characterId = MemoryReader::GetP1CharacterID();
            currentData.player1.character = MemoryReader::GetP1CharacterName();
        } else {
            currentData.player1.characterId = -1;
            currentData.player1.character = "Unknown";
        }
        
        // Check win counts less frequently (every 500ms)
        DWORD currentTime = GetTickCount();
        if (currentTime - lastWinCountCheck > WIN_COUNT_CHECK_INTERVAL_MS) {
            // Only get win counts periodically to reduce calls
            cachedP1WinCount = MemoryReader::GetP1WinCount();
            cachedP2WinCount = MemoryReader::GetP2WinCount();
            lastWinCountCheck = currentTime;
        }
        
        // Use cached win counts
        currentData.player1.winCount = cachedP1WinCount;
        
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
        
        // Use cached win counts
        currentData.player2.winCount = cachedP2WinCount;
        
        // Check if game is active based on valid character IDs
        currentData.gameActive = (currentData.player1.characterId >= 0 && 
                                currentData.player2.characterId >= 0);
        
        // Only perform update operations if data has changed
        bool hasDataChanged = false;
        
        // Check for changes in player 1 data
        if (currentData.player1.nickname != prevData.player1.nickname || 
            currentData.player1.character != prevData.player1.character ||
            currentData.player1.characterId != prevData.player1.characterId ||
            currentData.player1.winCount != prevData.player1.winCount) {
            
            hasDataChanged = true;
            
            // Log specific changes
            if (currentData.player1.character != prevData.player1.character) {
                Logger::Info("P1 character changed: " + 
                           (prevData.player1.character.empty() ? "None" : prevData.player1.character) + 
                           " -> " + currentData.player1.character);
            }
            
            if (currentData.player1.winCount != prevData.player1.winCount) {
                Logger::Info("P1 win count changed: " + std::to_string(prevData.player1.winCount) + 
                           " -> " + std::to_string(currentData.player1.winCount));
            }
        }
        
        // Check for changes in player 2 data
        if (currentData.player2.nickname != prevData.player2.nickname || 
            currentData.player2.character != prevData.player2.character ||
            currentData.player2.characterId != prevData.player2.characterId ||
            currentData.player2.winCount != prevData.player2.winCount) {
            
            hasDataChanged = true;
            
            // Log specific changes
            if (currentData.player2.character != prevData.player2.character) {
                Logger::Info("P2 character changed: " + 
                           (prevData.player2.character.empty() ? "None" : prevData.player2.character) + 
                           " -> " + currentData.player2.character);
            }
            
            if (currentData.player2.winCount != prevData.player2.winCount) {
                Logger::Info("P2 win count changed: " + std::to_string(prevData.player2.winCount) + 
                           " -> " + std::to_string(currentData.player2.winCount));
            }
        }
        
        // Check for game state changes
        bool wasGameActive = prevData.gameActive;
        currentData.gameActive = (currentData.player1.characterId >= 0 && currentData.player2.characterId >= 0);
        
        if (currentData.gameActive != wasGameActive) {
            hasDataChanged = true;
            Logger::Info("Game state changed: " + std::string(wasGameActive ? "active" : "inactive") + 
                       " -> " + std::string(currentData.gameActive ? "active" : "inactive"));
        }
        
        // If data changed, update the previousData member variable
        if (hasDataChanged) {
            previousData = currentData;
        }
        
        // Check if we're just now detecting characters
        bool isCharacterSelected = (currentData.player1.characterId >= 0 || currentData.player2.characterId >= 0);
        if (!wasCharacterSelected && isCharacterSelected) {
            // We just detected characters! Force a refresh of all data
            Logger::Info("Character selection detected - refreshing all data");
            MemoryReader::ForceRefreshCharacterData();
        }
        
        return hasDataChanged;
        
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

// Modify UpdateThreadProc to force cache clearing more aggressively
DWORD WINAPI GameDataManager::UpdateThreadProc(LPVOID lpParam) {
    Logger::Info("Game data update thread started");
    
    // Count consecutive failures for error reporting
    int consecutiveFailures = 0;
    DWORD updateCounter = 0;
    
    while (running) {
        // Force cache clearing on periodic intervals to detect character changes
        if (updateCounter % 15 == 0) {  // Every ~1.5 seconds
            // Changed from private ClearCache to public ForceRefreshCharacterData
            MemoryReader::ForceRefreshCharacterData();  // This ensures we detect character selection
        }
        
        bool shouldUpdateOverlay = false;
        
        // Only update overlay files when data has actually changed
        bool updateResult = Update();
        
        // Track failures for diagnostic purposes
        if (!updateResult) {
            consecutiveFailures++;
            
            // Log an error only if we're consistently failing
            if (consecutiveFailures == 10) {
                Logger::Warning("Ten consecutive update failures detected - possible memory reading issue");
            }
            
            // Slow down updates if we keep failing
            if (consecutiveFailures > 30) {
                Sleep(500);  // Increase delay when having issues
            }
        } else {
            // Reset counter on success
            if (consecutiveFailures > 0) {
                consecutiveFailures = 0;
            }
            
            // Mark for overlay update if data changed
            shouldUpdateOverlay = true;
        }
        
        // Force periodic updates even if data hasn't changed
        if (++updateCounter >= 50) { // Every ~5 seconds
            shouldUpdateOverlay = true;
            updateCounter = 0;
            
            // Force a periodic re-read of character data
            // This helps recover if we previously failed to detect characters
            Logger::Debug("Performing periodic refresh of game data");
        }
        
        if (shouldUpdateOverlay) {
            OverlayData::UpdateFiles();
        }
        
        Sleep(100);
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
