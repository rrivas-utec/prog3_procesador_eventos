#include "event_processor.h"
#include "execution_factory.h"
#include "event_logger.h"
#include "event_reader.h"
#include "event_creator_registry.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace {

// Serializa la salida por consola para evitar lineas intercaladas.
void print_line(std::mutex& output_mutex, const std::string& line) {
  std::lock_guard lock(output_mutex);
  std::cout << line << '\n';
}

// Construye el resumen final a partir de los contadores compartidos.
std::string summary_text(const statistics& stats, const std::string& mode) {
  std::ostringstream summary;
  summary << "SUMMARY | mode=" << mode << " | lines=" << stats.lines.load()
          << " accepted=" << stats.accepted.load() << " rejected=" << stats.rejected.load()
          << " processed=" << stats.processed.load() << " delivered=" << stats.delivered.load()
          << " dlq=" << stats.dlq.load() << " input_errors=" << stats.input_errors.load()
          << " dlq_errors=" << stats.dlq_errors.load();
  return summary.str();
}

// Busca un nombre de archivo que no exista en la carpeta indicada.
std::filesystem::path unique_path(
    const std::filesystem::path& directory, const std::string& prefix,
    std::atomic<unsigned long long>& sequence) {
  while (true) {
    const auto number = sequence.fetch_add(1) + 1;
    std::ostringstream file_name;
    file_name << prefix << std::setw(4) << std::setfill('0') << number << ".txt";
    std::filesystem::path candidate = directory / file_name.str();
    std::error_code error;
    const bool exists = std::filesystem::exists(candidate, error);
    if (error) return {};
    if (!exists) return candidate;
  }
}

// Conserva un mensaje rechazado o fallido con su origen y motivo.
bool save_dlq(const std::string& original, const std::filesystem::path& source_file,
              const std::size_t line_number, const std::string& reason, statistics& stats,
              std::atomic<unsigned long long>& sequence,
              std::filesystem::path& saved_path) {
  event_logger& logger = event_logger::instance();
  // Crea la DLQ si hace falta antes de reservar un nombre de archivo.
  const std::filesystem::path directory{"salida/dlq"};
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    ++stats.dlq_errors;
    logger.write("DLQ_ERROR | " + source_file.string() + ":" + std::to_string(line_number) +
                 " | " + error.message());
    return false;
  }
  saved_path = unique_path(directory, "failed_", sequence);
  if (saved_path.empty()) {
    ++stats.dlq_errors;
    logger.write("DLQ_ERROR | " + source_file.string() + ":" + std::to_string(line_number) +
                 " | no se pudo consultar un nombre disponible");
    return false;
  }
  std::ofstream output_file(saved_path, std::ios::binary);
  if (!output_file) {
    ++stats.dlq_errors;
    logger.write("DLQ_ERROR | " + source_file.string() + ":" + std::to_string(line_number) +
                 " | " + saved_path.string());
    return false;
  }
  // Incluye el texto recibido y su procedencia para facilitar la revision posterior.
  output_file << "Contenido original: " << original << '\n'
              << "Archivo de entrada: " << source_file.string() << '\n'
              << "Línea: " << line_number << '\n' << "Motivo: " << reason << '\n';
  output_file.close();
  if (!output_file) {
    ++stats.dlq_errors;
    logger.write("DLQ_ERROR | " + source_file.string() + ":" + std::to_string(line_number) +
                 " | error escribiendo " + saved_path.string());
    return false;
  }
  ++stats.dlq;
  return true;
}

// Entrega un evento validado; si falla, intenta conservarlo en la DLQ.
void process_event(const event& current_event,
                   const std::map<std::string, std::filesystem::path>& recipients,
                   result_tracker& tracker, statistics& stats,
                   std::atomic<unsigned long long>& output_sequence,
                   std::atomic<unsigned long long>& dlq_sequence,
                   std::mutex& output_mutex) {
  event_logger& logger = event_logger::instance();
  tracker.record_processed();
  const std::string action = current_event.type == "alerta"
      ? "alerta atendida" : "información registrada";
  logger.write("PROCESSED | " + current_event.id + " | result=ok | " + action);
  print_line(output_mutex, "PROCESSED | " + current_event.id + " | result=ok");

  // La ruta procede de la configuracion ya validada, no del texto de entrada.
  const std::filesystem::path& destination = recipients.at(current_event.recipient);
  std::error_code error;
  std::filesystem::create_directories(destination, error);
  std::filesystem::path target;
  bool delivered = false;
  if (!error) {
    target = unique_path(destination, "event_", output_sequence);
    if (!target.empty()) {
      if (std::ofstream output_file(target, std::ios::binary); output_file) {
        output_file << "id=" << current_event.id << '\n'
                    << "tipo=" << current_event.type << '\n'
                    << "severidad=" << current_event.severity << '\n'
                    << "destinatario=" << current_event.recipient << '\n'
                    << "archivo=" << current_event.source_file.string() << '\n'
                    << "linea=" << current_event.line_number << '\n'
                    << "mensaje=" << current_event.message << '\n';
        output_file.close();
        delivered = static_cast<bool>(output_file);
      }
    }
  }
  if (delivered) {
    ++stats.delivered;
    logger.write("DELIVERED | " + current_event.id + " | " + target.string());
    print_line(output_mutex, "DELIVERED | " + current_event.id + " | " + target.string());
    return;
  }

  // Si la entrega falla, conserva el evento en la DLQ con la causa disponible.
  const std::string reason = error ? "falló la carpeta de destino: " + error.message()
                                   : "no se pudo escribir el archivo de entrega";
  logger.write("DELIVERY_ERROR | " + current_event.id + " | " + reason);
  const std::string original = current_event.type + "|" + current_event.severity + "|" +
      current_event.recipient + "|" + current_event.message;
  std::filesystem::path dlq_path;
  const bool saved = save_dlq(original, current_event.source_file, current_event.line_number,
                              reason, stats, dlq_sequence, dlq_path);
  print_line(output_mutex, "DELIVERY_FAILED | " + current_event.id + " | " + reason + " | " +
                           (saved ? dlq_path.string() : "DLQ_ERROR"));
}

}  // namespace

// Recorre los TXT de la carpeta, valida sus lineas y procesa los eventos.
int process_input(const run_options& options,
                  const std::map<std::string, std::filesystem::path>& recipients,
                  statistics& stats) {
  event_logger& logger = event_logger::instance();
  std::mutex output_mutex;
  const auto factory = make_execution_factory(options.mode);
  auto tracker = factory->create_result_tracker(stats);
  std::error_code error;
  // Un directorio invalido se informa como error de entrada antes de recorrerlo.
  if (!std::filesystem::is_directory(options.input_directory, error) || error) {
    ++stats.input_errors;
    const std::string message = "INPUT_ERROR | " + options.input_directory.string() +
                                " | la carpeta no existe o no se puede leer";
    logger.write(message);
    print_line(output_mutex, message);
    const std::string summary = summary_text(stats, options.mode);
    print_line(output_mutex, summary);
    logger.write(summary);
    return 1;
  }

  // Reune los TXT y los ordena para obtener un recorrido reproducible.
  std::vector<std::filesystem::path> input_files;
  std::filesystem::directory_iterator iterator(options.input_directory, error);
  const std::filesystem::directory_iterator end;
  if (error) {
    ++stats.input_errors;
    const std::string message = "INPUT_ERROR | " + options.input_directory.string() +
                                " | no se pudo recorrer la carpeta: " + error.message();
    logger.write(message);
    print_line(output_mutex, message);
  } else {
    while (iterator != end) {
      std::error_code entry_error;
      // Ignora subcarpetas y procesa solo archivos TXT del directorio indicado.
      if (iterator->is_regular_file(entry_error) && !entry_error &&
          iterator->path().extension() == ".txt") {
        input_files.push_back(iterator->path());
      }
      iterator.increment(error);
      if (error) {
        ++stats.input_errors;
        const std::string message = "INPUT_ERROR | " + options.input_directory.string() +
                                    " | error recorriendo la carpeta: " + error.message();
        logger.write(message);
        print_line(output_mutex, message);
        break;
      }
    }
  }
  std::sort(input_files.begin(), input_files.end());

  // Secuencias compartidas para asignar identificadores y nombres unicos.
  std::atomic<unsigned long long> event_sequence{0};
  std::atomic<unsigned long long> output_sequence{0};
  std::atomic<unsigned long long> dlq_sequence{0};
  std::vector<event> accepted_events;
  // Usa el registro Singleton que contiene los creadores TXT y CSV.
  event_creator_registry& creator_registry = event_creator_registry::instance();
  // TXT es el formato predeterminado; basta cambiar esta clave para usar CSV.
  constexpr const char* input_format = "txt";
  std::unique_ptr<event_file_creator> creator =
      creator_registry.create_creator(input_format);
  for (const auto& source_file : input_files) {
    const event_import_result imported_file = creator->import_file(source_file, recipients);
    if (!imported_file.error.empty()) {
      ++stats.input_errors;
      const std::string message = "INPUT_ERROR | " + source_file.string() +
                                  " | " + imported_file.error;
      logger.write(message);
      print_line(output_mutex, message);
    }

    // El creador entrega cada linea no vacia junto con su parseo o motivo de rechazo.
    for (const auto& imported : imported_file.records) {
      ++stats.lines;
      if (!imported.parsed_event) {
        ++stats.rejected;
        std::filesystem::path saved_path;
        const bool stored = save_dlq(imported.original_line, source_file,
                                     imported.line_number, imported.reason,
                                     stats, dlq_sequence, saved_path);
        const std::string result = "REJECTED | " + source_file.string() + ":" +
            std::to_string(imported.line_number) + " | " + imported.reason + " | " +
            (stored ? saved_path.string() : "DLQ_ERROR");
        logger.write(result);
        print_line(output_mutex, result);
        continue;
      }

      event parsed_event = *imported.parsed_event;
      std::ostringstream event_id;
      event_id << "event_" << std::setw(4) << std::setfill('0')
               << event_sequence.fetch_add(1) + 1;
      parsed_event.id = event_id.str();
      ++stats.accepted;
      const std::string result = "ACCEPTED | " + source_file.string() + ":" +
          std::to_string(imported.line_number) + " | " + parsed_event.id + " | " +
          parsed_event.type + " | " + parsed_event.severity + " | " +
          parsed_event.recipient;
      logger.write(result);
      print_line(output_mutex, result);
      accepted_events.push_back(std::move(parsed_event));
    }
  }  // La Abstract Factory crea productos compatibles con el modo de ejecucion.
  auto dispatcher = factory->create_dispatcher(options.worker_count);
  auto shutdown = factory->create_shutdown_coordinator(*dispatcher);
  const auto handler = [&](const event& current_event) {
    process_event(current_event, recipients, *tracker, stats, output_sequence,
                  dlq_sequence, output_mutex);
  };
  dispatcher->dispatch(accepted_events, handler);
  shutdown->finish();

  const std::string summary = summary_text(stats, options.mode);
  print_line(output_mutex, summary);
  logger.write(summary);
  return stats.input_errors.load() == 0 ? 0 : 1;
}
