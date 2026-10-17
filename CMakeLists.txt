cmake_minimum_required(VERSION 4.3)
project(prog3_procesador_eventos)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(prog3_procesador_eventos
    src/main.cpp
    src/event.cpp
    src/event_reader.cpp
    src/event_creator_registry.cpp
    src/event_logger.cpp
    src/event_processor.cpp
    src/execution_factory.cpp
    src/sync_execution_factory.cpp
    src/async_execution_factory.cpp
    src/recipient_config.cpp
    src/run_options.cpp
)
target_include_directories(prog3_procesador_eventos PRIVATE include)
target_link_libraries(prog3_procesador_eventos PRIVATE Threads::Threads)

# Copia los datos de ejecucion junto al ejecutable despues de cada compilacion.
foreach(directory IN ITEMS entrada config target)
  add_custom_command(TARGET prog3_procesador_eventos POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/${directory}"
          "$<TARGET_FILE_DIR:prog3_procesador_eventos>/${directory}"
      COMMENT "Copying ${directory} to the executable directory"
  )
endforeach()
