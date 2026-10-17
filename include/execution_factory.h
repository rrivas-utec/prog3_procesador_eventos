#pragma once

#include "event.h"
#include "statistics.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Producto para despachar eventos de forma sincronica o concurrente.
class event_dispatcher {
 public:
  virtual ~event_dispatcher() = default;
  virtual void dispatch(const std::vector<event>& events,
                       const std::function<void(const event&)>& handler) = 0;
  virtual void close() = 0;
};

// Producto compartido que registra cuantos eventos terminaron su procesamiento.
class result_tracker {
 public:
  virtual ~result_tracker() = default;
  virtual void record_processed() = 0;
};

// Producto comun que delega el cierre en el dispatcher concreto.
class shutdown_coordinator {
 public:
  virtual ~shutdown_coordinator() = default;
  virtual void finish() = 0;
};

// Abstract Factory para crear los productos compatibles con un modo de ejecucion.
class execution_factory {
 public:
  virtual ~execution_factory() = default;
  [[nodiscard]] virtual std::unique_ptr<event_dispatcher> create_dispatcher(std::size_t worker_count) const = 0;
  virtual std::unique_ptr<result_tracker> create_result_tracker(statistics& stats) const = 0;
  virtual std::unique_ptr<shutdown_coordinator> create_shutdown_coordinator(
      event_dispatcher& dispatcher) const = 0;
};

// Las fabricas concretas crean dispatchers especificos de su modo.
class sync_execution_factory final : public execution_factory {
 public:
  [[nodiscard]] std::unique_ptr<event_dispatcher> create_dispatcher(std::size_t worker_count) const override;
  std::unique_ptr<result_tracker> create_result_tracker(statistics& stats) const override;
  std::unique_ptr<shutdown_coordinator> create_shutdown_coordinator(
      event_dispatcher& dispatcher) const override;
};

class async_execution_factory final : public execution_factory {
 public:
  [[nodiscard]] std::unique_ptr<event_dispatcher> create_dispatcher(std::size_t worker_count) const override;
  std::unique_ptr<result_tracker> create_result_tracker(statistics& stats) const override;
  std::unique_ptr<shutdown_coordinator> create_shutdown_coordinator(
      event_dispatcher& dispatcher) const override;
};

std::unique_ptr<execution_factory> make_execution_factory(const std::string& mode);
