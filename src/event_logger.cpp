#include "event_logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>

// Conserva una sola instancia y la inicializa al primer acceso.
event_logger& event_logger::instance(const std::string& file_path) {
  static event_logger logger(file_path);
  return logger;
}

// Crea la carpeta del log y abre el archivo para agregar registros.
event_logger::event_logger(const std::string& file_path) {
  const std::filesystem::path log_path(file_path);
  std::error_code error;
  std::filesystem::create_directories(log_path.parent_path(), error);
  if (!error) output_file_.open(log_path, std::ios::app);
  if (error || !output_file_) {
    std::cerr << "No se pudo abrir el log: " << file_path << '\n';
  }
}

// Protege cada escritura para que los workers no mezclen lineas del log.
void event_logger::write(const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!output_file_) return;
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  // Usa la conversion segura de hora local disponible en cada plataforma.
  std::tm local_time{};
#ifdef _WIN32
  const bool have_local_time = localtime_s(&local_time, &now) == 0;
#else
  const bool have_local_time = localtime_r(&now, &local_time) != nullptr;
#endif
  if (have_local_time)
    output_file_ << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << " | ";
  output_file_ << message << '\n';
  output_file_.flush();
}
