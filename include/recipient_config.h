#pragma once

#include <filesystem>
#include <map>
#include <string>

// Carga la relacion entre cada destinatario logico y su carpeta de salida.
bool load_recipients(const std::filesystem::path& file_path,
                     std::map<std::string, std::filesystem::path>& recipients,
                     std::string& reason);
