#include "event_processor.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace {

struct async_queue {
  std::mutex mutex;
  std::condition_variable ready;
  std::deque<event> events;
  bool closed{false};
};

void print_line(std::mutex& output_mutex, const std::string& line) {
  std::lock_guard<std::mutex> lock(output_mutex);
  std::cout << line << '\n';
}

std::string summary_text(const statistics& stats, const std::string& mode) {
  std::ostringstream summary;
  summary << "SUMMARY | mode=" << mode << " | lines=" << stats.lines.load()
          << " accepted=" << stats.accepted.load() << " rejected=" << stats.rejected.load()
          << " processed=" << stats.processed.load() << " delivered=" << stats.delivered.load()
          << " dlq=" << stats.dlq.load() << " input_errors=" << stats.input_errors.load()
          << " dlq_errors=" << stats.dlq_errors.load();
  return summary.str();
}

std::filesystem::path unique_path(
    const std::filesystem::path& directory, const std::string& prefix,
    std::atomic<unsigned long long>& sequence) {
  while (true) {
    const auto number = sequence.fetch_add(1) + 1;
    std::ostringstream file_name;
    file_name << prefix << std::setw(4) << std::setfill('0') << number << ".txt";
    const std::filesystem::path candidate = directory / file_name.str();
    std::error_code error;
    const bool exists = std::filesystem::exists(candidate, error);
    if (error) return {};
    if (!exists) return candidate;
  }
}

bool save_dlq(const std::string& original, const std::filesystem::path& source_file,
              std::size_t line_number, const std::string& reason, event_logger& logger,
              statistics& stats, std::atomic<unsigned long long>& sequence,
              std::filesystem::path& saved_path) {
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

void process_event(const event& current_event,
                   const std::map<std::string, std::filesystem::path>& recipients,
                   event_logger& logger, statistics& stats,
                   std::atomic<unsigned long long>& output_sequence,
                   std::atomic<unsigned long long>& dlq_sequence,
                   std::mutex& output_mutex) {
  ++stats.processed;
  const std::string action = current_event.type == "alerta"
      ? "alerta atendida" : "información registrada";
  logger.write("PROCESSED | " + current_event.id + " | result=ok | " + action);
  print_line(output_mutex, "PROCESSED | " + current_event.id + " | result=ok");

  const std::filesystem::path destination = recipients.at(current_event.recipient);
  std::error_code error;
  std::filesystem::create_directories(destination, error);
  std::filesystem::path target;
  bool delivered = false;
  if (!error) {
    target = unique_path(destination, "event_", output_sequence);
    if (!target.empty()) {
      std::ofstream output_file(target, std::ios::binary);
      if (output_file) {
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

  const std::string reason = error ? "falló la carpeta de destino: " + error.message()
                                   : "no se pudo escribir el archivo de entrega";
  logger.write("DELIVERY_ERROR | " + current_event.id + " | " + reason);
  const std::string original = current_event.type + "|" + current_event.severity + "|" +
      current_event.recipient + "|" + current_event.message;
  std::filesystem::path dlq_path;
  const bool saved = save_dlq(original, current_event.source_file, current_event.line_number,
                              reason, logger, stats, dlq_sequence, dlq_path);
  print_line(output_mutex, "DELIVERY_FAILED | " + current_event.id + " | " + reason + " | " +
                           (saved ? dlq_path.string() : "DLQ_ERROR"));
}

void process_synchronously(
    const std::vector<event>& events,
    const std::map<std::string, std::filesystem::path>& recipients,
    event_logger& logger, statistics& stats,
    std::atomic<unsigned long long>& output_sequence,
    std::atomic<unsigned long long>& dlq_sequence, std::mutex& output_mutex) {
  for (const event& current_event : events) {
    process_event(current_event, recipients, logger, stats, output_sequence,
                  dlq_sequence, output_mutex);
  }
}

void process_asynchronously(
    std::vector<event>& events,
    const std::map<std::string, std::filesystem::path>& recipients,
    const run_options& options, event_logger& logger, statistics& stats,
    std::atomic<unsigned long long>& output_sequence,
    std::atomic<unsigned long long>& dlq_sequence, std::mutex& output_mutex) {
  async_queue queue;
  std::vector<std::jthread> workers;
  workers.reserve(options.worker_count);
  for (std::size_t worker_index = 0; worker_index < options.worker_count; ++worker_index) {
    workers.emplace_back([&queue, &recipients, &logger, &stats, &output_sequence,
                          &dlq_sequence, &output_mutex]() {
      while (true) {
        event current_event;
        {
          std::unique_lock<std::mutex> lock(queue.mutex);
          queue.ready.wait(lock, [&queue]() { return queue.closed || !queue.events.empty(); });
          if (queue.events.empty() && queue.closed) return;
          current_event = std::move(queue.events.front());
          queue.events.pop_front();
        }
        process_event(current_event, recipients, logger, stats, output_sequence,
                      dlq_sequence, output_mutex);
      }
    });
  }
  {
    std::lock_guard<std::mutex> lock(queue.mutex);
    for (event& current_event : events) queue.events.push_back(std::move(current_event));
    queue.closed = true;
  }
  queue.ready.notify_all();
  workers.clear();
}

}  // namespace

int process_input(const run_options& options,
                  const std::map<std::string, std::filesystem::path>& recipients,
                  event_logger& logger, statistics& stats) {
  std::mutex output_mutex;
  std::ifstream input_file(options.input_file);
  if (!input_file) {
    ++stats.input_errors;
    logger.write("INPUT_ERROR | " + options.input_file.string() + " | no existe o no se puede abrir");
    print_line(output_mutex, "INPUT_ERROR | " + options.input_file.string() +
                             " | no existe o no se puede abrir");
    print_line(output_mutex, summary_text(stats, options.mode));
    return 1;
  }

  std::atomic<unsigned long long> event_sequence{0};
  std::atomic<unsigned long long> output_sequence{0};
  std::atomic<unsigned long long> dlq_sequence{0};
  std::vector<event> accepted_events;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input_file, line)) {
    ++line_number;
    if (trim(line).empty()) continue;
    ++stats.lines;
    std::string reason;
    auto parsed_event = parse_event(line, options.input_file, line_number, recipients, reason);
    if (!parsed_event) {
      ++stats.rejected;
      std::filesystem::path saved_path;
      const bool stored = save_dlq(line, options.input_file, line_number, reason,
                                   logger, stats, dlq_sequence, saved_path);
      const std::string result = "REJECTED | " + options.input_file.string() + ":" +
          std::to_string(line_number) + " | " + reason + " | " +
          (stored ? saved_path.string() : "DLQ_ERROR");
      logger.write(result);
      print_line(output_mutex, result);
      continue;
    }
    std::ostringstream event_id;
    event_id << "event_" << std::setw(4) << std::setfill('0') << event_sequence.fetch_add(1) + 1;
    parsed_event->id = event_id.str();
    ++stats.accepted;
    const std::string result = "ACCEPTED | " + options.input_file.string() + ":" +
        std::to_string(line_number) + " | " + parsed_event->id + " | " + parsed_event->type +
        " | " + parsed_event->severity + " | " + parsed_event->recipient;
    logger.write(result);
    print_line(output_mutex, result);
    accepted_events.push_back(std::move(*parsed_event));
  }
  if (input_file.bad()) {
    ++stats.input_errors;
    logger.write("INPUT_ERROR | " + options.input_file.string() + " | error durante la lectura");
    print_line(output_mutex, "INPUT_ERROR | " + options.input_file.string() +
                             " | error durante la lectura");
  }

  if (options.mode == "sync") {
    process_synchronously(accepted_events, recipients, logger, stats, output_sequence,
                          dlq_sequence, output_mutex);
  } else {
    process_asynchronously(accepted_events, recipients, options, logger, stats,
                           output_sequence, dlq_sequence, output_mutex);
  }
  const std::string summary = summary_text(stats, options.mode);
  print_line(output_mutex, summary);
  logger.write(summary);
  return stats.input_errors.load() == 0 ? 0 : 1;
}
