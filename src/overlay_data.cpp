#include "../include/overlay_data.h"
#include "../include/game_data.h"
#include "../include/logger.h"
#include <filesystem>
#include <windows.h>

std::string OverlayData::outputDirectory = "";
bool OverlayData::initialized = false;

// HTML template with responsive design
const std::string htmlOverlayTemplate = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EFZ Streaming Overlay</title>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="overlay-container">
        <!-- Player 1 Side -->
        <div class="player-side left">
            <div class="character-portrait" id="p1-portrait">
                <img id="p1-portrait-img" src="portraits/unknown.png" alt="P1 Character">
            </div>
        </div>

        <!-- Scoreboard -->
        <div class="scoreboard">
            <div class="player-info p1">
                <div class="player-name" id="p1-name">Player 1</div>
                <div class="character-name" id="p1-character">Unknown</div>
            </div>
            
            <div class="score-display">
                <div class="score p1" id="p1-score">0</div>
                <div class="score-divider">-</div>
                <div class="score p2" id="p2-score">0</div>
            </div>
            
            <div class="player-info p2">
                <div class="player-name" id="p2-name">Player 2</div>
                <div class="character-name" id="p2-character">Unknown</div>
            </div>
        </div>

        <!-- Player 2 Side -->
        <div class="player-side right">
            <div class="character-portrait" id="p2-portrait">
                <img id="p2-portrait-img" src="portraits/unknown.png" alt="P2 Character">
            </div>
        </div>
    </div>

    <script src="overlay.js"></script>
</body>
</html>)";

// CSS with clean styling
const std::string cssTemplate = R"(* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Roboto', sans-serif;
    background-color: transparent;
    overflow: hidden;
    width: 100vw;
    height: 100vh;
}

.overlay-container {
    width: 100%;
    height: 100%;
    display: flex;
    justify-content: center;
    align-items: center;
    position: relative;
}

/* Character Portraits */
.player-side {
    position: absolute;
    top: 50%;
    transform: translateY(-50%);
}

.left {
    left: 0;
}

.right {
    right: 0;
}

.character-portrait {
    width: 200px;
    height: 300px;
    overflow: hidden;
    position: relative;
}

.character-portrait img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    transition: transform 0.3s ease-out;
}

/* Add horizontal flip for P2 */
#p2-portrait img {
    transform: scaleX(-1); /* Flip horizontally */
}

/* Portrait Animations */
.portrait-enter {
    animation: portrait-slide-in 0.6s ease forwards;
}

/* Player-specific positioning */
.player-side.left {
    left: 20px;
}

.player-side.right {
    right: 20px;
}

/* Scoreboard */
.scoreboard {
    background: rgba(0, 0, 0, 0.8);
    border: 2px solid #ffdd57;
    border-radius: 8px;
    padding: 10px 30px;
    color: white;
    display: flex;
    justify-content: space-between;
    align-items: center;
    width: 500px;
    height: 80px;
    position: absolute;
    top: 30px;
    left: 50%;
    transform: translateX(-50%);
    box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
}

.player-info {
    display: flex;
    flex-direction: column;
    width: 180px;
    text-align: center;
    overflow: hidden;
}

.player-name {
    font-size: 20px;
    font-weight: bold;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.5);
}

.character-name {
    font-size: 16px;
    color: #ffdd57;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.5);
}

.score-display {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
    font-size: 28px;
    font-weight: bold;
}

.score {
    width: 40px;
    text-align: center;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.5);
}

.p1 .score {
    color: #5d9cec;
}

.p2 .score {
    color: #fc6e51;
}

.score-divider {
    color: #aaa;
}

/* P1/P2 specific colors */
.player-info.p1 .player-name {
    color: #5d9cec;
}

.player-info.p2 .player-name {
    color: #fc6e51;
}

/* Animations */
@keyframes portrait-slide-in {
    0% {
        opacity: 0;
        transform: translateX(-30px);
    }
    100% {
        opacity: 1;
        transform: translateX(0);
    }
}

/* Right-side portrait animation correction */
.player-side.right .portrait-enter {
    animation: portrait-slide-in-right 0.6s ease forwards;
}

@keyframes portrait-slide-in-right {
    0% {
        opacity: 0;
        transform: translateX(30px);
    }
    100% {
        opacity: 1;
        transform: translateX(0);
    }
}

/* Data change animations */
.flash {
    animation: flash-animation 0.5s;
}

@keyframes flash-animation {
    0% { background-color: rgba(255, 255, 255, 0); }
    50% { background-color: rgba(255, 255, 255, 0.3); }
    100% { background-color: rgba(255, 255, 255, 0); }
})";

// JavaScript with data sanitization and error handling
const std::string jsTemplate = R"(// Character mappings for EFZ
const CHARACTER_DATA = {
    // Character ID to name and portrait filename mapping
    0: { name: "Akane", portrait: "akane.png" },
    1: { name: "Akiko", portrait: "akiko.png" },
    2: { name: "Ikumi", portrait: "ikumi.png" },
    3: { name: "Misaki", portrait: "misaki.png" },
    4: { name: "Sayuri", portrait: "sayuri.png" },
    5: { name: "Kanna", portrait: "kanna.png" },
    6: { name: "Kaori", portrait: "kaori.png" },
    7: { name: "Makoto", portrait: "makoto.png" },
    8: { name: "Minagi", portrait: "minagi.png" },
    9: { name: "Mio", portrait: "mio.png" },
    10: { name: "Mishio", portrait: "mishio.png" },
    11: { name: "Misuzu", portrait: "misuzu.png" },
    12: { name: "Mizuka", portrait: "mizuka.png" },
    13: { name: "Nagamori", portrait: "nagamori.png" },
    14: { name: "Nanase", portrait: "nanase.png" },
    15: { name: "ExNanase", portrait: "exnanase.png" },
    16: { name: "Nayuki", portrait: "nayuki.png" },
    17: { name: "NayukiB", portrait: "nayukib.png" },
    18: { name: "Shiori", portrait: "shiori.png" },
    19: { name: "Ayu", portrait: "ayu.png" },
    20: { name: "Mai", portrait: "mai.png" },
    21: { name: "Mayu", portrait: "mayu.png" },
    22: { name: "MizukaB", portrait: "mizukab.png" },
    23: { name: "Kano", portrait: "kano.png" }
};

// Store previous state to detect changes
let prevState = {
    player1: { nickname: "", character: "", characterId: -1, winCount: 0 },
    player2: { nickname: "", character: "", characterId: -1, winCount: 0 },
    gameActive: false
};

// Sanitize and validate player nicknames
function sanitizeNickname(nickname) {
    if (!nickname || nickname === 'undefined' || nickname.trim() === '') {
        return 'Player';
    }
    
    // Replace any potential unsafe characters and limit length
    const sanitized = nickname.replace(/[^\w\s\-_.!?+#@$%&*(){}\[\]:;<>,'"\\|\/~^]/g, '_');
    return sanitized.substring(0, 16);
}

// Validate win count to prevent corruption
function sanitizeWinCount(count) {
    // If count is not a number or outside reasonable range
    if (isNaN(count) || count < 0 || count > 99) {
        return 0;
    }
    return count;
}

// Function to update the UI based on game data
function updateOverlay(data) {
    try {
        // Sanitize player nicknames
        const p1Nickname = sanitizeNickname(data.player1.nickname);
        const p2Nickname = sanitizeNickname(data.player2.nickname);
        
        // Update player names
        const p1NameElement = document.getElementById('p1-name');
        const p2NameElement = document.getElementById('p2-name');
        
        if (p1Nickname !== prevState.player1.nickname) {
            p1NameElement.textContent = p1Nickname;
            p1NameElement.classList.add('flash');
            setTimeout(() => p1NameElement.classList.remove('flash'), 500);
        }
        
        if (p2Nickname !== prevState.player2.nickname) {
            p2NameElement.textContent = p2Nickname;
            p2NameElement.classList.add('flash');
            setTimeout(() => p2NameElement.classList.remove('flash'), 500);
        }

        // Update scores
        const p1ScoreElement = document.getElementById('p1-score');
        const p2ScoreElement = document.getElementById('p2-score');
        
        // Sanitize win counts
        const p1WinCount = sanitizeWinCount(data.player1.winCount);
        const p2WinCount = sanitizeWinCount(data.player2.winCount);
        
        if (p1WinCount !== prevState.player1.winCount) {
            p1ScoreElement.textContent = p1WinCount;
            p1ScoreElement.classList.add('flash');
            setTimeout(() => p1ScoreElement.classList.remove('flash'), 500);
        }
        
        if (p2WinCount !== prevState.player2.winCount) {
            p2ScoreElement.textContent = p2WinCount;
            p2ScoreElement.classList.add('flash');
            setTimeout(() => p2ScoreElement.classList.remove('flash'), 500);
        }

        // Update character info
        if (data.player1.characterId !== prevState.player1.characterId) {
            updateCharacter(1, data.player1.characterId);
        }
        
        if (data.player2.characterId !== prevState.player2.characterId) {
            updateCharacter(2, data.player2.characterId);
        }

        // Store current state (with sanitized values)
        prevState = {
            player1: {
                nickname: p1Nickname,
                character: data.player1.character,
                characterId: data.player1.characterId,
                winCount: p1WinCount
            },
            player2: {
                nickname: p2Nickname,
                character: data.player2.character,
                characterId: data.player2.characterId,
                winCount: p2WinCount
            },
            gameActive: data.gameActive
        };
    } catch (err) {
        console.error("Error updating overlay:", err);
    }
}

// Function to update character info and portrait
function updateCharacter(playerNum, characterId) {
    try {
        const charNameElement = document.getElementById(`p${playerNum}-character`);
        const portraitElement = document.getElementById(`p${playerNum}-portrait-img`);
        
        // Get character data or use unknown if invalid
        const characterData = CHARACTER_DATA[characterId] || { name: "Unknown", portrait: "unknown.png" };
        
        // Update character name
        charNameElement.textContent = characterData.name;
        charNameElement.classList.add('flash');
        setTimeout(() => charNameElement.classList.remove('flash'), 500);
        
        // Update portrait with animation
        const oldPortrait = portraitElement.src;
        const newPortrait = `portraits/${characterData.portrait}`;
        
        if (!oldPortrait.endsWith(newPortrait)) {
            // Fade out and in for portrait change
            portraitElement.style.opacity = '0';
            
            setTimeout(() => {
                portraitElement.src = newPortrait;
                portraitElement.style.opacity = '1';
                portraitElement.classList.add('portrait-enter');
                setTimeout(() => portraitElement.classList.remove('portrait-enter'), 600);
            }, 300);
        }
    } catch (err) {
        console.error(`Error updating character for player ${playerNum}:`, err);
    }
}

// Function to fetch data from the server with error handling and rate limiting
function fetchGameData() {
    // Use /api/data endpoint explicitly
    fetch('http://localhost:8080/api/data')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            updateOverlay(data);
            // Schedule next update with reasonable timing
            setTimeout(fetchGameData, 500); // 500ms is plenty fast for UI updates
        })
        .catch(error => {
            console.warn('Error fetching game data:', error);
            // If we can't connect, try again less frequently to avoid resource drain
            setTimeout(fetchGameData, 3000);
        });
}

// Initialize the overlay
window.addEventListener('DOMContentLoaded', () => {
    console.log("EFZ Streaming Overlay loaded");
    fetchGameData();
});)";

// README file for portraits folder
const std::string portraitsReadme = R"(Place character portrait image files here, named as follows:

akane.png, akiko.png, ikumi.png, misaki.png, sayuri.png, kanna.png,
kaori.png, makoto.png, minagi.png, mio.png, mishio.png, misuzu.png,
mizuka.png, nagamori.png, nanase.png, exnanase.png, nayuki.png, nayukib.png,
shiori.png, ayu.png, mai.png, mayu.png, mizukab.png, kano.png, unknown.png

Recommended portrait dimensions: 200x300 pixels
)";

// Debug HTML template - moved up to be with other templates
const std::string debugHtmlTemplate = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EFZ Overlay Debug</title>
    <style>
        body { 
            background: #222; 
            color: #eee; 
            font-family: monospace;
            padding: 20px;
        }
        .data-panel {
            background: #333;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
        }
        .portrait {
            border: 2px solid #555;
            max-width: 200px;
            max-height: 300px;
            margin: 10px;
        }
        button {
            padding: 8px 16px;
            background: #555;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>EFZ Streaming Overlay Debug</h1>
    
    <div class="data-panel">
        <h2>Connection Status: <span id="connection-status">Checking...</span></h2>
        <button id="refresh-btn">Refresh Data</button>
        <button id="clear-log">Clear Console</button>
    </div>
    
    <div class="data-panel">
        <h2>Current Game Data:</h2>
        <pre id="game-data">Loading...</pre>
    </div>
    
    <div class="data-panel">
        <h2>Player 1 Portrait:</h2>
        <div>Character: <span id="p1-char">Unknown</span> (ID: <span id="p1-id">-1</span>)</div>
        <img id="p1-portrait" class="portrait" src="" alt="P1 Portrait">
    </div>
    
    <div class="data-panel">
        <h2>Player 2 Portrait:</h2>
        <div>Character: <span id="p2-char">Unknown</span> (ID: <span id="p2-id">-1</span>)</div>
        <img id="p2-portrait" class="portrait" src="" alt="P2 Portrait">
    </div>
    
    <script>
        let connectionOK = false;
        let lastUpdated = new Date();
        const connectionStatus = document.getElementById('connection-status');
        const gameData = document.getElementById('game-data');
        const refreshBtn = document.getElementById('refresh-btn');
        const clearLogBtn = document.getElementById('clear-log');
        const p1Portrait = document.getElementById('p1-portrait');
        const p2Portrait = document.getElementById('p2-portrait');
        const p1Char = document.getElementById('p1-char');
        const p2Char = document.getElementById('p2-char');
        const p1Id = document.getElementById('p1-id');
        const p2Id = document.getElementById('p2-id');
        
        // Update connection status
        function updateConnectionStatus() {
            const now = new Date();
            const diff = (now - lastUpdated) / 1000;
            
            if (diff > 5) {
                connectionStatus.textContent = 'Disconnected';
                connectionStatus.style.color = 'red';
                connectionOK = false;
            } else {
                connectionStatus.textContent = 'Connected';
                connectionStatus.style.color = 'green';
                connectionOK = true;
            }
        }
        
        // Fetch game data from server
        function fetchGameData() {
            fetch('http://localhost:8080/api/data')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.json();
                })
                .then(data => {
                    lastUpdated = new Date();
                    updateConnectionStatus();
                    
                    // Format JSON with indentation
                    gameData.textContent = JSON.stringify(data, null, 2);
                    
                    // Update portraits
                    if (data.player1) {
                        p1Char.textContent = data.player1.character;
                        p1Id.textContent = data.player1.characterId;
                        
                        if (data.player1.characterId >= 0) {
                            const charName = data.player1.character.toLowerCase();
                            p1Portrait.src = `portraits/${charName}.png`;
                            p1Portrait.onerror = () => {
                                console.warn(`Portrait not found: ${charName}.png`);
                                p1Portrait.src = 'portraits/unknown.png';
                            };
                        } else {
                            p1Portrait.src = 'portraits/unknown.png';
                        }
                    }
                    
                    if (data.player2) {
                        p2Char.textContent = data.player2.character;
                        p2Id.textContent = data.player2.characterId;
                        
                        if (data.player2.characterId >= 0) {
                            const charName = data.player2.character.toLowerCase();
                            p2Portrait.src = `portraits/${charName}.png`;
                            p2Portrait.onerror = () => {
                                console.warn(`Portrait not found: ${charName}.png`);
                                p2Portrait.src = 'portraits/unknown.png';
                            };
                        } else {
                            p2Portrait.src = 'portraits/unknown.png';
                        }
                    }
                    
                    // Schedule next update
                    setTimeout(fetchGameData, 1000); // 1s is plenty for debug page
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    connectionStatus.textContent = 'Connection Error';
                    connectionStatus.style.color = 'red';
                    connectionOK = false;
                    
                    // Try again with backoff
                    setTimeout(fetchGameData, 3000);
                });
        }
        
        // Initialize
        refreshBtn.addEventListener('click', fetchGameData);
        clearLogBtn.addEventListener('click', () => console.clear());
        
        // Start fetching data
        fetchGameData();
        
        // Update connection status every 2 seconds
        setInterval(updateConnectionStatus, 2000);
    </script>
</body>
</html>)";

bool OverlayData::Initialize() {
    // Get DLL path to locate assets
    char dllPathBuffer[MAX_PATH];
    // Use the DLL's HINSTANCE instead of NULL to get our DLL path
    HMODULE hModule = GetModuleHandleA("efz_streaming_overlay.dll");
    if (!hModule) {
        // Fallback to current module if specific name fails
        hModule = (HMODULE)GetModuleHandleA(NULL);
        Logger::Warning("Could not get DLL module handle, using fallback method");
    }
    
    GetModuleFileNameA(hModule, dllPathBuffer, MAX_PATH);
    Logger::Info("DLL path: " + std::string(dllPathBuffer));
    
    // Convert to std::filesystem path and get directory
    std::filesystem::path dllPath(dllPathBuffer);
    std::filesystem::path assetsDir = dllPath.parent_path() / "overlay_assets";
    outputDirectory = assetsDir.string();
    
    // Make sure the overlay assets directory exists
    if (!std::filesystem::exists(assetsDir)) {
        std::filesystem::create_directory(assetsDir);
        Logger::Info("Output directory created: " + outputDirectory);
    }

    // Create portraits directory if it doesn't exist
    std::filesystem::path portraitsDir = assetsDir / "portraits";
    if (!std::filesystem::exists(portraitsDir)) {
        std::filesystem::create_directory(portraitsDir);
        Logger::Info("Portraits directory created: " + portraitsDir.string());
    }

    // Write the overlay files (HTML, CSS, JS) to the output directory
    WriteToFile(assetsDir / "overlay.html", htmlOverlayTemplate);
    WriteToFile(assetsDir / "style.css", cssTemplate);
    WriteToFile(assetsDir / "overlay.js", jsTemplate);
    WriteToFile(portraitsDir / "README.txt", portraitsReadme);
    
    // Initialize the JSON data structure
    ResetData();
    
    // Create placeholders for individual text files as well
    WriteToFile(assetsDir / "p1_nickname.txt", "Player 1");
    WriteToFile(assetsDir / "p2_nickname.txt", "Player 2");
    WriteToFile(assetsDir / "p1_character.txt", "Unknown");
    WriteToFile(assetsDir / "p2_character.txt", "Unknown");
    WriteToFile(assetsDir / "p1_wins.txt", "0");
    WriteToFile(assetsDir / "p2_wins.txt", "0");
    
    // Also create a debug.html file to help troubleshoot
    WriteToFile(assetsDir / "debug.html", debugHtmlTemplate);
    Logger::Info("Created debug.html page for troubleshooting");
    
    initialized = true;
    return true;
}

void OverlayData::UpdateFiles() {
    LOG_FUNCTION_ENTRY();
    const GameData& data = GameDataManager::GetCurrentData();
    
    // Write individual text files for easy OBS text source integration
    WriteToFile(std::filesystem::path(outputDirectory) / "p1_nickname.txt", data.player1.nickname);
    WriteToFile(std::filesystem::path(outputDirectory) / "p2_nickname.txt", data.player2.nickname);
    WriteToFile(std::filesystem::path(outputDirectory) / "p1_character.txt", data.player1.character);
    WriteToFile(std::filesystem::path(outputDirectory) / "p2_character.txt", data.player2.character);
    WriteToFile(std::filesystem::path(outputDirectory) / "p1_wins.txt", std::to_string(data.player1.winCount));
    WriteToFile(std::filesystem::path(outputDirectory) / "p2_wins.txt", std::to_string(data.player2.winCount));
    
    // Write combined JSON file for web sources
    WriteToFile(std::filesystem::path(outputDirectory) / "game_data.json", data.ToJSON());
    
    // Debug the data we're writing
    if (data.player1.characterId >= 0 || data.player2.characterId >= 0) {
        Logger::Info("Writing character data - P1: " + data.player1.character + 
                    " (ID:" + std::to_string(data.player1.characterId) + "), P2: " + 
                    data.player2.character + " (ID:" + std::to_string(data.player2.characterId) + ")");
    }
    
    LOG_FUNCTION_EXIT();
}

void OverlayData::Shutdown() {
    Logger::Info("Overlay data shutdown");
}

void OverlayData::ResetData() {
    // Reset any game data to default values
    Logger::Debug("Resetting overlay data to default values");
    // Your reset implementation here
}

bool OverlayData::WriteToFile(const std::filesystem::path& filePath, const std::string& content) {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            Logger::Error("Failed to open file for writing: " + filePath.string());
            return false;
        }
        file << content;
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("Error writing to file " + filePath.string() + ": " + e.what());
        return false;
    }
}
