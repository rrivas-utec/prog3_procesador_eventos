#pragma once

#include "event.h"
#include "run_options.h"
#include "statistics.h"

#include <filesystem>
#include <map>
#include <string>

// Recorre la carpeta de entrada, valida los TXT y despacha los eventos aceptados.
int process_input(const run_options& options,
                  const std::map<std::string, std::filesystem::path>& recipients,
                  statistics& stats);
