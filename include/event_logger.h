#pragma once

#include <fstream>
#include <mutex>
#include <string>

// Logger compartido por todos los componentes y protegido para uso concurrente.
class event_logger {
 public:
  // Devuelve la instancia unica del logger para toda la ejecucion.
  static event_logger& instance(const std::string& file_path = "logs/events.log");
  void write(const std::string& message);

 private:
  explicit event_logger(const std::string& file_path);
  std::ofstream output_file_;
  std::mutex mutex_;
};
