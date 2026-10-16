#pragma once

#include <filesystem>
#include <map>
#include <string>

bool load_recipients(const std::filesystem::path& file_path,
                     std::map<std::string, std::filesystem::path>& recipients,
                     std::string& reason);
