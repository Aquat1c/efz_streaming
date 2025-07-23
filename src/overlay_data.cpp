#include "../include/overlay_data.h"
#include "../include/game_data.h"
#include "../include/logger.h"
#include "../include/constants.h" // Ensure constants are included
#include <string>
#include <fstream>
#include <filesystem>
#include <windows.h>

// Extern declaration to get the DLL's base address
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

std::string OverlayData::outputDirectory = "";
bool OverlayData::initialized = false;


// README file for portraits folder
const std::string portraitsReadme = R"END_OF_STRING(Place character portrait image files here, named as follows:

akane.png, akiko.png, ikumi.png, misaki.png, sayuri.png, kanna.png,
kaori.png, makoto.png, minagi.png, mio.png, mishio.png, misuzu.png,
mizuka.png, nagamori.png, nanase.png, exnanase.png, nayuki.png, nayukib.png,
shiori.png, ayu.png, mai.png, mayu.png, mizukab.png, kano.png, unknown.png

Recommended portrait dimensions: 200x300 pixels
)END_OF_STRING";



bool OverlayData::Initialize() {
    LOG_FUNCTION_ENTRY();
    
    // Get the path of the current DLL, regardless of its name
    char dllPath[MAX_PATH];
    GetModuleFileNameA((HMODULE)&__ImageBase, dllPath, MAX_PATH);
    
    // The output directory will be a subdirectory next to the DLL
    outputDirectory = (std::filesystem::path(dllPath).parent_path() / "overlay_assets").string();
    
    // Create the output directory if it doesn't exist
    if (!std::filesystem::exists(outputDirectory)) {
        if (!std::filesystem::create_directories(outputDirectory)) {
            Logger::Error("Failed to create output directory: " + outputDirectory);
            return false;
        }
    }
    
    // Create portraits subdirectory
    std::filesystem::path portraitsDir = std::filesystem::path(outputDirectory) / "portraits";
    if (!std::filesystem::exists(portraitsDir)) {
        std::filesystem::create_directories(portraitsDir);
    }
    
    // Create a README in the portraits folder
    WriteToFile(portraitsDir / "README.txt", "Place character portrait PNG files in this directory.");
    
    // Create placeholder text files
    ResetData();

    initialized = true;
    Logger::Info("Overlay data manager initialized. Outputting files to: " + outputDirectory);
    LOG_FUNCTION_EXIT();
    return true;
}

void OverlayData::UpdateFiles() {
    const GameData& data = GameDataManager::GetCurrentData();
    static int lastP1CharId = -1;
    static int lastP2CharId = -1;

    // Get the absolute path to the assets directory
    std::filesystem::path assetsPath = std::filesystem::absolute(outputDirectory);

    // Update text files for nicknames, characters, and wins
    WriteToFile(assetsPath / "p1_nickname.txt", data.player1.nickname);
    WriteToFile(assetsPath / "p2_nickname.txt", data.player2.nickname);
    WriteToFile(assetsPath / "p1_character.txt", data.player1.character);
    WriteToFile(assetsPath / "p2_character.txt", data.player2.character);
    WriteToFile(assetsPath / "p1_wins.txt", std::to_string(data.player1.winCount));
    WriteToFile(assetsPath / "p2_wins.txt", std::to_string(data.player2.winCount));

    // --- Logic for hot-swapping portrait files ---
    if (data.player1.characterId != lastP1CharId) {
        std::filesystem::path portraitsPath = assetsPath / "portraits";
        std::string p1_portrait_file = "unknown.png";
        if (data.player1.characterId >= 0 && data.player1.characterId <= MAX_CHARACTER_ID) {
            p1_portrait_file = CHARACTER_PORTRAITS[data.player1.characterId];
        }
        
        try {
            std::filesystem::path sourceFile = portraitsPath / p1_portrait_file;
            if (std::filesystem::exists(sourceFile)) {
                std::filesystem::copy(sourceFile, assetsPath / "p1_portrait.png", std::filesystem::copy_options::overwrite_existing);
            }
            lastP1CharId = data.player1.characterId;
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::Error("Failed to copy P1 portrait: " + std::string(e.what()));
        }
    }

    if (data.player2.characterId != lastP2CharId) {
        std::filesystem::path portraitsPath = assetsPath / "portraits";
        std::string p2_portrait_file = "unknown.png";
        if (data.player2.characterId >= 0 && data.player2.characterId <= MAX_CHARACTER_ID) {
            p2_portrait_file = CHARACTER_PORTRAITS[data.player2.characterId];
        }
        
        try {
            std::filesystem::path sourceFile = portraitsPath / p2_portrait_file;
            if (std::filesystem::exists(sourceFile)) {
                std::filesystem::copy(sourceFile, assetsPath / "p2_portrait.png", std::filesystem::copy_options::overwrite_existing);
            }
            lastP2CharId = data.player2.characterId;
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::Error("Failed to copy P2 portrait: " + std::string(e.what()));
        }
    }
}

void OverlayData::Shutdown() {
    if (!initialized) {
        return;
    }

    Logger::Info("Shutting down overlay data manager and cleaning up generated files...");

    try {
        if (!outputDirectory.empty() && std::filesystem::exists(outputDirectory)) {
            const std::vector<std::string> filesToDelete = {
                "p1_nickname.txt", "p2_nickname.txt",
                "p1_character.txt", "p2_character.txt",
                "p1_wins.txt", "p2_wins.txt",
                "p1_portrait.png", "p2_portrait.png"
            };

            for (const auto& filename : filesToDelete) {
                std::filesystem::path filePath = std::filesystem::path(outputDirectory) / filename;
                if (std::filesystem::exists(filePath)) {
                    std::filesystem::remove(filePath);
                    Logger::Debug("Deleted file: " + filePath.string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::Error("Error during file cleanup: " + std::string(e.what()));
    }

    initialized = false;
    Logger::Info("Overlay data manager shut down.");
}

void OverlayData::ResetData() {
    std::filesystem::path assetsDir(outputDirectory);
    
    // Create placeholders for individual text files
    WriteToFile(assetsDir / "p1_nickname.txt", "Player 1");
    WriteToFile(assetsDir / "p2_nickname.txt", "Player 2");
    WriteToFile(assetsDir / "p1_character.txt", "Unknown");
    WriteToFile(assetsDir / "p2_character.txt", "Unknown");
    WriteToFile(assetsDir / "p1_wins.txt", "0");
    WriteToFile(assetsDir / "p2_wins.txt", "0");
    
    // Create placeholder portrait files by copying unknown.png if it exists
    // (Assuming unknown.png is provided by the user in the portraits folder)
    std::filesystem::path unknownPortrait = assetsDir / "portraits" / "unknown.png";
    if (std::filesystem::exists(unknownPortrait)) {
        try {
            std::filesystem::copy(unknownPortrait, assetsDir / "p1_portrait.png", std::filesystem::copy_options::overwrite_existing);
            std::filesystem::copy(unknownPortrait, assetsDir / "p2_portrait.png", std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::Error("Failed to create placeholder portraits: " + std::string(e.what()));
        }
    }
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
