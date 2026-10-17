#include "event_creator_registry.h"

#include <stdexcept>
#include <utility>

// Registra TXT y CSV una sola vez cuando se crea la instancia compartida.
event_creator_registry::event_creator_registry() {
  register_creator("txt", [] {
    return std::make_unique<txt_event_creator>();
  });
  register_creator("csv", [] {
    return std::make_unique<csv_event_creator>();
  });
}

// Devuelve la instancia unica; su inicializacion local es segura entre threads.
event_creator_registry& event_creator_registry::instance() {
  static event_creator_registry registry;
  return registry;
}

// Registra una fabrica para una clave, reemplazando la anterior si existe.
void event_creator_registry::register_creator(std::string format,
                                              creator_factory factory) {
  std::lock_guard lock(mutex_);
  factories_.insert_or_assign(std::move(format), std::move(factory));
}

// Copia la fabrica protegida y crea el objeto fuera de la seccion critica.
std::unique_ptr<event_file_creator> event_creator_registry::create_creator(
    const std::string& format) const {
  creator_factory factory;
  {
    std::lock_guard lock(mutex_);
    const auto found = factories_.find(format);
    if (found == factories_.end()) {
      throw std::invalid_argument("formato de archivo no registrado: " + format);
    }
    factory = found->second;
  }
  return factory();
}
