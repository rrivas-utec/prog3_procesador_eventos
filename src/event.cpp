#include "event.h"

#include <algorithm>
#include <cctype>

std::string trim(const std::string& value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
    return std::isspace(character) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
    return std::isspace(character) != 0;
  }).base();
  return first >= last ? std::string{} : std::string(first, last);
}

std::optional<event> parse_event(
    const std::string& line, const std::filesystem::path& source_file,
    std::size_t line_number, const std::map<std::string, std::filesystem::path>& recipients,
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

  event parsed_event{
      "",
      trim(line.substr(0, first_separator)),
      trim(line.substr(first_separator + 1, second_separator - first_separator - 1)),
      trim(line.substr(second_separator + 1, third_separator - second_separator - 1)),
      trim(line.substr(third_separator + 1)),
      source_file,
      line_number};

  if (parsed_event.type.empty() || parsed_event.severity.empty() ||
      parsed_event.recipient.empty() || parsed_event.message.empty()) {
    reason = "hay un campo obligatorio vacío";
  } else if (parsed_event.type != "alerta" && parsed_event.type != "informacion") {
    reason = "tipo inválido: " + parsed_event.type;
  } else if (parsed_event.severity != "baja" && parsed_event.severity != "media" &&
             parsed_event.severity != "alta") {
    reason = "severidad inválida: " + parsed_event.severity;
  } else if (!recipients.contains(parsed_event.recipient)) {
    reason = "destinatario desconocido: " + parsed_event.recipient;
  } else {
    return parsed_event;
  }
  return std::nullopt;
}
