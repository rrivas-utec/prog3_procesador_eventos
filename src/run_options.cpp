#include "run_options.h"

#include <iostream>
#include <thread>

namespace {

// Convierte y valida la cantidad de workers permitida.
bool parse_worker_count(const std::string& text, std::size_t& worker_count) {
  try {
    std::size_t characters_used = 0;
    // Verifica el texto completo y limita el numero de workers para evitar valores extremos.
    const auto parsed_count = std::stoull(text, &characters_used);
    if (characters_used != text.size() || parsed_count == 0 || parsed_count > 256) return false;
    worker_count = static_cast<std::size_t>(parsed_count);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace

// Interpreta la carpeta de entrada, el modo y la cantidad de workers.
bool parse_options(int argc, char* argv[], run_options& options, int& exit_code) {
  exit_code = 2;
  // La ayuda termina correctamente sin iniciar el procesamiento.
  if (argc > 1 && std::string(argv[1]) == "--help") {
    std::cout << "Uso: prog3_procesador_eventos [carpeta] [sync|async] [workers]\n"
                 "Por defecto: entrada sync\n";
    exit_code = 0;
    return false;
  }
  if (argc > 4) {
    std::cerr << "Uso: prog3_procesador_eventos [carpeta] [sync|async] [workers]\n";
    return false;
  }
  if (argc >= 2) options.input_directory = argv[1];
  if (argc >= 3) options.mode = argv[2];
  if (options.mode != "sync" && options.mode != "async") {
    std::cerr << "El modo debe ser sync o async.\n";
    return false;
  }
  if (argc >= 4 && !parse_worker_count(argv[3], options.worker_count)) {
    std::cerr << "Workers debe ser un entero entre 1 y 256.\n";
    return false;
  }
  // Si no se indico cantidad, usa los hilos disponibles del equipo.
  if (argc == 1) {
    const auto available_workers = std::thread::hardware_concurrency();
    options.worker_count = available_workers == 0 ? 2 : available_workers;
  }
  exit_code = 0;
  return true;
}
