#include "event_logger.h"
#include "event_processor.h"
#include "recipient_config.h"
#include "run_options.h"
#include "statistics.h"

#include <iostream>
#include <map>
#include <string>

int main(int argc, char* argv[]) {
  run_options options;
  int exit_code = 0;
  if (!parse_options(argc, argv, options, exit_code)) return exit_code;

  event_logger logger("logs/events.log");
  statistics stats;
  std::map<std::string, std::filesystem::path> recipients;
  std::string config_error;
  if (!load_recipients("config/recipients.txt", recipients, config_error)) {
    logger.write("CONFIG_ERROR | " + config_error);
    std::cerr << "CONFIG_ERROR | " << config_error << '\n';
    return 1;
  }
  return process_input(options, recipients, logger, stats);
}
