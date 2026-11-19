
#pragma once 

#include <string>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include <iostream>
#include <fstream>
#include <ctime>
#include "Common/FileUtil.h"
#include "Common/FileSearch.h"

class Logger{

public:
    // Constructor
    Logger(std::string in_file_name){
        // No ':' in file names (Windows)
        std::time_t unix_time = std::time(nullptr);
        char time[32];
        std::strftime(time, sizeof(time), "%Y%m%dT%H%M%S", std::localtime(&unix_time));
        log_file_path = File::GetUserPath(D_STATELOGGER_IDX) + time + '_' + in_file_name + ".txt";
    };    
    
    // Path to log file
    std::string log_file_path;

    void writeToFile(std::string in_string) {
        File::WriteStringToFile(log_file_path, in_string + '\n', true);
    };

    void writeToTerminal(std::string in_string) {
        std::cout << in_string + "\n";
    };
};

