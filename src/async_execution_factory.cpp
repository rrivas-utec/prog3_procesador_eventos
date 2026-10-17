#include "execution_factory.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace {

struct async_queue {
  std::mutex mutex;
  std::condition_variable ready;
  std::deque<event> events;
  bool closed{false};
};

class asynchronous_dispatcher final : public event_dispatcher {
 public:
  explicit asynchronous_dispatcher(const std::size_t worker_count)
      : worker_count_(worker_count == 0 ? 2 : worker_count) {}

  // Inicia los workers y publica el lote para su procesamiento concurrente.
  void dispatch(const std::vector<event>& events,
                const std::function<void(const event&)>& handler) override {
    handler_ = handler;
    workers_.reserve(worker_count_);
    for (std::size_t index = 0; index < worker_count_; ++index) {
      workers_.emplace_back([this]() {
        while (true) {
          event current_event;
          {
            std::unique_lock<std::mutex> lock(queue_.mutex);
            queue_.ready.wait(lock, [this]() {
              return queue_.closed || !queue_.events.empty();
            });
            if (queue_.events.empty() && queue_.closed) return;
            current_event = std::move(queue_.events.front());
            queue_.events.pop_front();
          }
          handler_(current_event);
        }
      });
    }

    {
      std::lock_guard<std::mutex> lock(queue_.mutex);
      for (const event& current_event : events) queue_.events.push_back(current_event);
    }
    queue_.ready.notify_all();
  }

  // Cierra la cola y espera a que los workers terminen los eventos pendientes.
  void close() override {
    {
      std::lock_guard<std::mutex> lock(queue_.mutex);
      queue_.closed = true;
    }
    queue_.ready.notify_all();
    workers_.clear();
  }

  ~asynchronous_dispatcher() override { close(); }

 private:
  std::size_t worker_count_;
  async_queue queue_;
  std::vector<std::jthread> workers_;
  std::function<void(const event&)> handler_;
};

}  // namespace

// La familia asincronica crea un dispatcher con cola y workers.
std::unique_ptr<event_dispatcher>
async_execution_factory::create_dispatcher(const std::size_t worker_count) const {
  return std::make_unique<asynchronous_dispatcher>(worker_count);
}
