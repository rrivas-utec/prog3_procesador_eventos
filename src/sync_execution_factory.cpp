#include "execution_factory.h"

namespace {

class synchronous_dispatcher final : public event_dispatcher {
 public:
  void dispatch(const std::vector<event>& events,
                const std::function<void(const event&)>& handler) override {
    for (const event& current_event : events) handler(current_event);
  }

  void close() override {}
};

}  // namespace

// La familia sincronica procesa cada evento antes de devolver el control.
std::unique_ptr<event_dispatcher>
sync_execution_factory::create_dispatcher(std::size_t) const {
  return std::make_unique<synchronous_dispatcher>();
}
