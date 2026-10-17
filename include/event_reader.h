#pragma once

#include "event.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Resultado de una fila importada, con el texto original y su validacion.
struct imported_event {
  std::string original_line;
  std::size_t line_number{};
  std::optional<event> parsed_event;
  std::string reason;
};

struct event_import_result {
  std::filesystem::path source_file;
  std::vector<imported_event> records;
  std::string error;
};

// Producto que interpreta una linea segun el formato del archivo.
class event_line_reader {
 public:
  virtual ~event_line_reader() = default;
  virtual std::optional<std::vector<std::string>> parse_fields(
      const std::string& line, std::string& reason) const = 0;
};

// Creador base: mantiene el flujo de importacion y delega la creacion del lector.
class event_file_creator {
 public:
  virtual ~event_file_creator() = default;
  [[nodiscard]] event_import_result import_file(
      const std::filesystem::path& file_path,
      const std::map<std::string, std::filesystem::path>& recipients) const;

 protected:
  [[nodiscard]] virtual std::unique_ptr<event_line_reader> create_reader() const = 0;
};

class txt_event_creator final : public event_file_creator {
 protected:
  [[nodiscard]] std::unique_ptr<event_line_reader> create_reader() const override;
};

class csv_event_creator final : public event_file_creator {
 protected:
  [[nodiscard]] std::unique_ptr<event_line_reader> create_reader() const override;
};
