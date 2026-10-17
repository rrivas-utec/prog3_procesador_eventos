#include "event_reader.h"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

class txt_line_reader final : public event_line_reader {
 public:
  std::optional<std::vector<std::string>> parse_fields(
      const std::string& line, std::string& reason) const override {
    std::vector<std::string> fields;
    std::size_t field_start = 0;

    // Extrae los tres primeros campos usando |; el resto de la linea es el mensaje.
    for (int field_index = 0; field_index < 3; ++field_index) {
      const std::size_t separator = line.find('|', field_start);
      if (separator == std::string::npos) {
        reason = "se esperaban cuatro campos separados por |";
        return std::nullopt;
      }
      fields.push_back(line.substr(field_start, separator - field_start));
      field_start = separator + 1;
    }

    // Conserva cualquier | adicional dentro del ultimo campo, que es el mensaje.
    fields.push_back(line.substr(field_start));
    return fields;
  }
};

class csv_line_reader final : public event_line_reader {
 public:
  std::optional<std::vector<std::string>> parse_fields(
      const std::string& line, std::string& reason) const override {
    std::vector<std::string> fields;
    std::string field;
    bool inside_quotes = false;

    // Recorre el registro para distinguir comas separadoras de comas entre comillas.
    for (std::size_t index = 0; index < line.size(); ++index) {
      if (const char character = line[index]; character == '"') {
        // En CSV, dos comillas dentro de un campo entrecomillado representan una comilla.
        if (inside_quotes && index + 1 < line.size() && line[index + 1] == '"') {
          field.push_back('"');
          ++index;
        } else {
          inside_quotes = !inside_quotes;
        }
      } else if (character == ',' && !inside_quotes) {
        // Una coma fuera de comillas termina el campo actual.
        fields.push_back(field);
        field.clear();
      } else {
        field.push_back(character);
      }
    }

    // Un campo que sigue entre comillas indica que el registro CSV esta incompleto.
    if (inside_quotes) {
      reason = "comillas sin cerrar en el registro CSV";
      return std::nullopt;
    }

    fields.push_back(field);
    if (fields.size() != 4) {
      reason = "se esperaban cuatro campos CSV separados por coma";
      return std::nullopt;
    }
    return fields;
  }
};

}  // namespace

// Comparte apertura, lectura, procedencia y validacion; solo cambia el lector.
event_import_result event_file_creator::import_file(
    const std::filesystem::path& file_path,
    const std::map<std::string, std::filesystem::path>& recipients) const {
  event_import_result result;
  result.source_file = file_path;

  // Si no se puede abrir el archivo, devuelve el error junto con su ruta de origen.
  std::ifstream input_file(file_path);
  if (!input_file) {
    result.error = "no existe o no se puede abrir el archivo";
    return result;
  }

  // La subclase elige el lector adecuado; el flujo de importacion es comun.
  const std::unique_ptr<event_line_reader> reader = create_reader();
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input_file, line)) {
    ++line_number;

    // Ignora lineas vacias sin alterar la numeracion real del archivo.
    if (trim(line).empty()) continue;

    imported_event imported{line, line_number, std::nullopt, {}};
    if (auto fields = reader->parse_fields(line, imported.reason)) {
      // Reutiliza event_builder para validar los valores ya separados por formato.
      imported.parsed_event = event_builder(file_path, line_number, recipients)
          .set_type((*fields)[0])
          .set_severity((*fields)[1])
          .set_recipient((*fields)[2])
          .set_message((*fields)[3])
          .build(imported.reason);
    }

    // Guarda tanto eventos validos como invalidos para que el llamador los procese.
    result.records.push_back(std::move(imported));
  }

  // Distingue un fin de archivo normal de un fallo de lectura.
  if (input_file.bad()) result.error = "error durante la lectura del archivo";
  return result;
}

// Factory Method para seleccionar el lector que interpreta archivos TXT.
std::unique_ptr<event_line_reader> txt_event_creator::create_reader() const {
  return std::make_unique<txt_line_reader>();
}

// Factory Method para seleccionar el lector que interpreta archivos CSV.
std::unique_ptr<event_line_reader> csv_event_creator::create_reader() const {
  return std::make_unique<csv_line_reader>();
}
