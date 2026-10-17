#include "event_logger.h"
#include "event_processor.h"
#include "recipient_config.h"
#include "run_options.h"
#include "statistics.h"

#include <iostream>
#include <map>
#include <string>

int main(int argc, char* argv[]) {
  // Lee los argumentos de ejecución y termina si son inválidos o se solicitó ayuda.
  run_options options;
  if (int exit_code = 0; !parse_options(argc, argv, options, exit_code)) return exit_code;

  // Prepara el registro, los contadores y las rutas de entrega configuradas.
  event_logger& logger = event_logger::instance("logs/events.log");
  statistics stats;
  std::map<std::string, std::filesystem::path> recipients;
  if (std::string config_error; !load_recipients("config/recipients.txt", recipients, config_error)) {
    // Informa el error de configuración tanto en el log como en la consola.
    logger.write("CONFIG_ERROR | " + config_error);
    std::cerr << "CONFIG_ERROR | " << config_error << '\n';
    return 1;
  }
  // Procesa la entrada y devuelve el resultado como código de salida.
  return process_input(options, recipients, stats);
}
