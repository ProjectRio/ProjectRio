#include "GeckoCodeGenerator.h"
#include <filesystem>
#include <iostream>

void GeckoCodeGenerator::WriteGeneratedCode(
    const std::string& game_id,
    const std::string& code_name,
    const std::vector<std::string>& lines)
{
    // Build the path to User/GameSettings/<game_id>.ini
    std::filesystem::path user_dir = "User/GameSettings";
    if (!std::filesystem::exists(user_dir))
    {
        // create directory if it doesn't exist
        std::filesystem::create_directories(user_dir);
    }

    std::filesystem::path ini_path = user_dir / (game_id + ".ini");

    std::ofstream file(ini_path, std::ios::app);
    if (!file.is_open())
    {
        std::cerr << "Failed to open INI file: " << ini_path << std::endl;
        return;
    }

    // Write the [Gecko] section
    file << "\n[Gecko]\n";
    file << "$" << code_name << "\n";

    for (const auto& line : lines)
        file << line << "\n";

    // Write the [Gecko_Enabled] section
    file << "\n[Gecko_Enabled]\n";
    file << "$" << code_name << "\n";

    file.close();
}