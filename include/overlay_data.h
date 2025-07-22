#pragma once
#include <string>
#include <fstream>

class OverlayData {
public:
    static bool Initialize();
    static void UpdateFiles();
    static void Shutdown();
    
private:
    static std::string outputDirectory;
    static bool WriteToFile(const std::string& filename, const std::string& content);
};
