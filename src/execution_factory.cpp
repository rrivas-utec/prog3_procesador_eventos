#include "execution_factory.h"

namespace {

class atomic_result_tracker final : public result_tracker {
 public:
  explicit atomic_result_tracker(statistics& stats) : stats_(stats) {}

  void record_processed() override { ++stats_.processed; }

 private:
  statistics& stats_;
};

class dispatcher_shutdown_coordinator final : public shutdown_coordinator {
 public:
  explicit dispatcher_shutdown_coordinator(event_dispatcher& dispatcher)
      : dispatcher_(dispatcher) {}

  void finish() override { dispatcher_.close(); }

 private:
  event_dispatcher& dispatcher_;
};

}  // namespace

// Ambos modos pueden reutilizar el mismo seguimiento atomico y cierre por interfaz.
std::unique_ptr<result_tracker> sync_execution_factory::create_result_tracker(
    statistics& stats) const {
  return std::make_unique<atomic_result_tracker>(stats);
}

std::unique_ptr<shutdown_coordinator>
sync_execution_factory::create_shutdown_coordinator(
    event_dispatcher& dispatcher) const {
  return std::make_unique<dispatcher_shutdown_coordinator>(dispatcher);
}

std::unique_ptr<result_tracker> async_execution_factory::create_result_tracker(
    statistics& stats) const {
  return std::make_unique<atomic_result_tracker>(stats);
}

std::unique_ptr<shutdown_coordinator>
async_execution_factory::create_shutdown_coordinator(
    event_dispatcher& dispatcher) const {
  return std::make_unique<dispatcher_shutdown_coordinator>(dispatcher);
}

// Selecciona la fabrica concreta correspondiente al modo de procesamiento.
std::unique_ptr<execution_factory> make_execution_factory(const std::string& mode) {
  if (mode == "async") return std::make_unique<async_execution_factory>();
  return std::make_unique<sync_execution_factory>();
}
