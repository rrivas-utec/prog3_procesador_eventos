#pragma once

#include <atomic>
#include <cstddef>

struct statistics {
  std::atomic<std::size_t> lines{0};
  std::atomic<std::size_t> accepted{0};
  std::atomic<std::size_t> rejected{0};
  std::atomic<std::size_t> processed{0};
  std::atomic<std::size_t> delivered{0};
  std::atomic<std::size_t> dlq{0};
  std::atomic<std::size_t> input_errors{0};
  std::atomic<std::size_t> dlq_errors{0};
};
