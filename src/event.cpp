#include "event.h"

#include <algorithm>
#include <cctype>

// Elimina espacios en blanco al inicio y al final del texto.
std::string trim(const std::string& value) {
  const auto first = std::ranges::find_if_not(value, [](const unsigned char character) {
    return std::isspace(character) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char character) {
    return std::isspace(character) != 0;
  }).base();
  return first >= last ? std::string{} : std::string(first, last);
}

// Inicializa la procedencia que se conserva en el evento construido.
event_builder::event_builder(
    const std::filesystem::path& source_file, const std::size_t line_number,
    const std::map<std::string, std::filesystem::path>& recipients)
    : recipients_(recipients) {
  event_.source_file = source_file;
  event_.line_number = line_number;
}

event_builder& event_builder::set_type(std::string type) {
  event_.type = trim(type);
  return *this;
}

event_builder& event_builder::set_severity(std::string severity) {
  event_.severity = trim(severity);
  return *this;
}

event_builder& event_builder::set_recipient(std::string recipient) {
  event_.recipient = trim(recipient);
  return *this;
}

event_builder& event_builder::set_message(std::string message) {
  event_.message = trim(message);
  return *this;
}

// Rechaza el estado incompleto o incompatible antes de exponer el evento.
std::optional<event> event_builder::build(std::string& reason) const {
  if (event_.type.empty() || event_.severity.empty() || event_.recipient.empty() || event_.message.empty()) {
    reason = "hay un campo obligatorio vacio";
  } else if (event_.type != "alerta" && event_.type != "informacion") {
    reason = "tipo invalido: " + event_.type;
  } else if (event_.severity != "baja" && event_.severity != "media" && event_.severity != "alta") {
    reason = "severidad invalida: " + event_.severity;
  } else if (!recipients_.contains(event_.recipient)) {
    reason = "destinatario desconocido: " + event_.recipient;
  } else {
    return event_;
  }
  return std::nullopt;
}

// Separa los cuatro campos y delega su construccion y validacion al Builder.
std::optional<event> parse_event(
    const std::string& line,
    const std::filesystem::path& source_file,
    const std::size_t line_number,
    const std::map<std::string, std::filesystem::path>& recipients,
    std::string& reason) {
  const auto first_separator = line.find('|');
  const auto second_separator = first_separator == std::string::npos
      ? first_separator : line.find('|', first_separator + 1);
  const auto third_separator = second_separator == std::string::npos
      ? second_separator : line.find('|', second_separator + 1);
  if (third_separator == std::string::npos) {
    reason = "se esperaban cuatro campos";
    return std::nullopt;
  }

  return event_builder(source_file, line_number, recipients)
      .set_type(line.substr(0, first_separator))
      .set_severity(line.substr(first_separator + 1, second_separator - first_separator - 1))
      .set_recipient(line.substr(second_separator + 1, third_separator - second_separator - 1))
      .set_message(line.substr(third_separator + 1))
      .build(reason);
}
