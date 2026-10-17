#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

// Datos validados de un mensaje y su procedencia en el archivo de entrada.
struct event {
  std::string id;
  std::string type;
  std::string severity;
  std::string recipient;
  std::string message;
  std::filesystem::path source_file;
  std::size_t line_number{};
};

// Builder que valida los campos antes de entregar un evento completo.
class event_builder {
 public:
  event_builder(const std::filesystem::path& source_file, std::size_t line_number,
                const std::map<std::string, std::filesystem::path>& recipients);

  event_builder& set_type(std::string type);
  event_builder& set_severity(std::string severity);
  event_builder& set_recipient(std::string recipient);
  event_builder& set_message(std::string message);
  std::optional<event> build(std::string& reason) const;

 private:
  event event_;
  const std::map<std::string, std::filesystem::path>& recipients_;
};

// Normaliza espacios externos y valida una linea como evento.
std::string trim(const std::string& value);
std::optional<event> parse_event(
    const std::string& line, const std::filesystem::path& source_file,
    std::size_t line_number, const std::map<std::string, std::filesystem::path>& recipients,
    std::string& reason);
