#include "event_logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>

event_logger::event_logger(const std::string& file_path) {
  const std::filesystem::path log_path(file_path);
  std::error_code error;
  std::filesystem::create_directories(log_path.parent_path(), error);
  if (!error) output_file_.open(log_path, std::ios::app);
  if (error || !output_file_) {
    std::cerr << "No se pudo abrir el log: " << file_path << '\n';
  }
}

void event_logger::write(const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!output_file_) return;
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const std::tm* local_time = std::localtime(&now);
  if (local_time) output_file_ << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << " | ";
  output_file_ << message << '\n';
  output_file_.flush();
}
