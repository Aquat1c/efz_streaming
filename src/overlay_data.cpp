#include "../include/overlay_data.h"
#include "../include/game_data.h"
#include "../include/logger.h"
#include <filesystem>
#include <windows.h>

std::string OverlayData::outputDirectory = "";

bool OverlayData::Initialize() {
    Logger::Info("Initializing overlay data output");
    
    // Create output directory in the same folder as the DLL
    char dllPath[MAX_PATH];
    GetModuleFileNameA(nullptr, dllPath, MAX_PATH);
    
    std::filesystem::path path(dllPath);
    outputDirectory = path.parent_path().string() + "\\overlay_data";
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(outputDirectory);
        Logger::Info("Output directory created: " + outputDirectory);
        return true;
    } catch (const std::exception& e) {
        Logger::Error("Failed to create output directory: " + std::string(e.what()));
        return false;
    }
}

void OverlayData::UpdateFiles() {
    const GameData& data = GameDataManager::GetCurrentData();
    
    // Write individual text files for easy OBS text source integration
    WriteToFile("p1_nickname.txt", data.player1.nickname);
    WriteToFile("p2_nickname.txt", data.player2.nickname);
    WriteToFile("p1_character.txt", data.player1.character);
    WriteToFile("p2_character.txt", data.player2.character);
    WriteToFile("p1_wins.txt", std::to_string(data.player1.winCount));
    WriteToFile("p2_wins.txt", std::to_string(data.player2.winCount));
    
    // Write combined JSON file for web sources
    WriteToFile("game_data.json", data.ToJSON());
    
    // Write a simple HTML overlay template
    std::string htmlOverlay = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { 
            background: transparent; 
            font-family: Arial, sans-serif; 
            color: white;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.8);
        }
        .player-info { 
            display: inline-block; 
            margin: 10px; 
            padding: 10px;
            background: rgba(0,0,0,0.5);
            border-radius: 5px;
        }
        .wins { font-size: 24px; font-weight: bold; }
        .nickname { font-size: 18px; }
        .character { font-size: 16px; color: #ffff00; }
    </style>
</head>
<body>
    <div class="player-info">
        <div class="nickname" id="p1-nickname">Player 1</div>
        <div class="character" id="p1-character">Character</div>
        <div class="wins" id="p1-wins">0</div>
    </div>
    <div class="player-info">
        <div class="nickname" id="p2-nickname">Player 2</div>
        <div class="character" id="p2-character">Character</div>
        <div class="wins" id="p2-wins">0</div>
    </div>
    
    <script>
        function updateData() {
            fetch('http://localhost:8080')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('p1-nickname').textContent = data.player1.nickname;
                    document.getElementById('p1-character').textContent = data.player1.character;
                    document.getElementById('p1-wins').textContent = data.player1.winCount;
                    
                    document.getElementById('p2-nickname').textContent = data.player2.nickname;
                    document.getElementById('p2-character').textContent = data.player2.character;
                    document.getElementById('p2-wins').textContent = data.player2.winCount;
                })
                .catch(error => console.error('Error:', error));
        }
        
        // Update every 100ms
        setInterval(updateData, 100);
        updateData(); // Initial load
    </script>
</body>
</html>
)";
    
    WriteToFile("overlay.html", htmlOverlay);
}

void OverlayData::Shutdown() {
    Logger::Info("Overlay data shutdown");
}

bool OverlayData::WriteToFile(const std::string& filename, const std::string& content) {
    std::string fullPath = outputDirectory + "\\" + filename;
    
    std::ofstream file(fullPath);
    if (file.is_open()) {
        file << content;
        file.close();
        return true;
    }
    
    Logger::Error("Failed to write file: " + fullPath);
    return false;
}
