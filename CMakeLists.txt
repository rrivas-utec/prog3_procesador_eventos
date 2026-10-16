cmake_minimum_required(VERSION 4.3)
project(prog3_procesador_eventos)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(prog3_procesador_eventos
    src/main.cpp
    src/event.cpp
    src/event_logger.cpp
    src/event_processor.cpp
    src/recipient_config.cpp
    src/run_options.cpp
)
target_include_directories(prog3_procesador_eventos PRIVATE include)
target_link_libraries(prog3_procesador_eventos PRIVATE Threads::Threads)
