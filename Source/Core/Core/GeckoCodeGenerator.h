#pragma once

#include <string>   // for std::string
#include <vector>   // for std::vector
#include <fstream>  // optional if you use ofstream here

class GeckoCodeGenerator
{
public:
    // static function to write a generated Gecko code to the game INI
    static void WriteGeneratedCode(
        const std::string& game_id,
        const std::string& code_name,
        const std::vector<std::string>& lines);
};