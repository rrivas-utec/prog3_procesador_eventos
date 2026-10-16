#pragma once

#include "event.h"
#include "event_logger.h"
#include "run_options.h"
#include "statistics.h"

#include <filesystem>
#include <map>
#include <string>

int process_input(const run_options& options,
                  const std::map<std::string, std::filesystem::path>& recipients,
                  event_logger& logger, statistics& stats);
