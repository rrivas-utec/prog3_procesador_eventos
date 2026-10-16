#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

struct run_options {
  std::filesystem::path input_file{"entrada/messages_01.txt"};
  std::string mode{"sync"};
  std::size_t worker_count{2};
};

bool parse_options(int argc, char* argv[], run_options& options, int& exit_code);
