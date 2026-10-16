#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

struct event {
  std::string id;
  std::string type;
  std::string severity;
  std::string recipient;
  std::string message;
  std::filesystem::path source_file;
  std::size_t line_number{};
};

std::string trim(const std::string& value);
std::optional<event> parse_event(
    const std::string& line, const std::filesystem::path& source_file,
    std::size_t line_number, const std::map<std::string, std::filesystem::path>& recipients,
    std::string& reason);
