#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// Parametros que controlan la carpeta de entrada y el procesamiento.
struct run_options {
  std::filesystem::path input_directory{"entrada"};
  std::string mode{"sync"};
  std::size_t worker_count{2};
};

// Valida los argumentos de consola y completa estas opciones.
bool parse_options(int argc, char* argv[], run_options& options, int& exit_code);
