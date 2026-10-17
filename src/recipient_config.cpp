#include "recipient_config.h"

#include "event.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

// Acepta nombres simples que puedan usarse como identificadores de destinatario.
bool valid_identifier(const std::string& value) {
  return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
    return std::isalnum(character) != 0 || character == '_' || character == '-';
  });
}

// Restringe las rutas de entrega a sub-carpetas dentro de salida/destinatarios.
bool valid_destination(const std::filesystem::path& path) {
  if (path.is_absolute()) return false;
  // Normaliza segmentos como . y .. antes de comprobar la carpeta permitida.
  const std::filesystem::path normalized_path = path.lexically_normal();
  auto part = normalized_path.begin();
  if (part == normalized_path.end() || *part++ != "salida" ||
      part == normalized_path.end() || *part++ != "destinatarios" ||
      part == normalized_path.end()) return false;
  return std::none_of(normalized_path.begin(), normalized_path.end(),
                      [](const std::filesystem::path& item) { return item == ".."; });
}

}  // namespace

// Lee, valida y carga el mapa de destinatarios desde el archivo de configuracion.
bool load_recipients(const std::filesystem::path& file_path,
                     std::map<std::string, std::filesystem::path>& recipients,
                     std::string& reason) {
  std::ifstream input_file(file_path);
  if (!input_file) {
    reason = "No se pudo abrir la configuración: " + file_path.string();
    return false;
  }

  std::string line;
  std::size_t line_number = 0;
  // Cada linea util debe asociar un identificador con una sola carpeta.
  while (std::getline(input_file, line)) {
    ++line_number;
    const std::string content = trim(line);
    if (content.empty() || content.front() == '#') continue;
    // Se exige un solo separador para que la configuracion tenga dos campos.
    const auto separator = line.find('|');
    if (separator == std::string::npos || line.find('|', separator + 1) != std::string::npos) {
      reason = "Formato inválido en " + file_path.string() + ":" + std::to_string(line_number);
      return false;
    }
    const std::string name = trim(line.substr(0, separator));
    const std::filesystem::path destination = trim(line.substr(separator + 1));
    if (!valid_identifier(name) || !valid_destination(destination)) {
      reason = "Identificador o carpeta inválidos en " + file_path.string() + ":" +
               std::to_string(line_number);
      return false;
    }
    // El mapa tambien permite detectar nombres repetidos.
    if (!recipients.emplace(name, destination).second) {
      reason = "Destinatario duplicado en " + file_path.string() + ":" +
               std::to_string(line_number);
      return false;
    }
  }
  if (input_file.bad() || recipients.empty()) {
    reason = input_file.bad() ? "Error leyendo " + file_path.string()
                              : "No hay destinatarios configurados";
    return false;
  }
  return true;
}
