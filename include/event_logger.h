#pragma once

#include <fstream>
#include <mutex>
#include <string>

class event_logger {
 public:
  explicit event_logger(const std::string& file_path);
  void write(const std::string& message);

 private:
  std::ofstream output_file_;
  std::mutex mutex_;
};
