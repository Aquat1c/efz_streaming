#pragma once
#include <string>
#include <fstream>
#include <filesystem>

class OverlayData {
public:
    static bool Initialize();
    static void UpdateFiles();
    static void Shutdown();
    static void ResetData(); 
    static std::string GetOutputDirectory() { return outputDirectory; } // Add this line
    
private:
    static std::string outputDirectory;
    static bool initialized;
    
    // Update the WriteToFile signature to accept std::filesystem::path
    static bool WriteToFile(const std::filesystem::path& filePath, const std::string& content);
};
