#pragma once

#include "event_reader.h"

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

// Registro compartido que asocia claves de formato con fabricas de creadores.
class event_creator_registry {
 public:
  using creator_factory = std::function<std::unique_ptr<event_file_creator>()>;

  static event_creator_registry& instance();
  void register_creator(std::string format, creator_factory factory);
  [[nodiscard]] std::unique_ptr<event_file_creator> create_creator(
      const std::string& format) const;

  event_creator_registry(const event_creator_registry&) = delete;
  event_creator_registry& operator=(const event_creator_registry&) = delete;

 private:
  event_creator_registry();

  mutable std::mutex mutex_;
  std::unordered_map<std::string, creator_factory> factories_;
};
