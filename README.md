# Procesador de eventos

Implementacion en C++20 para recorrer una carpeta de archivos TXT, validar sus eventos, procesarlos en modo sincronico o asincronico y registrar entregas y fallos.

## Patrones creacionales

| Patron | Clases nuevas | Clases y archivos modificados | Uso |
| --- | --- | --- | --- |
| **Singleton** | - | `event_logger` en `include/event_logger.h` y `src/event_logger.cpp`; uso desde `src/main.cpp` y `process_input` en `src/event_processor.cpp` | `event_logger::instance()` conserva una instancia compartida. El constructor es privado y el logger protege las escrituras con un mutex. |
| **Builder** | `event_builder` en `include/event.h`, implementado en `src/event.cpp` | `parse_event` en `src/event.cpp` | Los metodos `set_type`, `set_severity`, `set_recipient` y `set_message` reunen los datos. `build` valida campos, tipo, severidad y destinatario antes de devolver el evento. |
| **Abstract Factory** | Interfaces `execution_factory`, `event_dispatcher`, `result_tracker` y `shutdown_coordinator` en `include/execution_factory.h`. El selector comun esta en `src/execution_factory.cpp`; cada familia concreta tiene su propio .cpp: `src/sync_execution_factory.cpp` y `src/async_execution_factory.cpp`. | `process_input` en `src/event_processor.cpp` selecciona y utiliza la familia del modo indicado. `CMakeLists.txt` incorpora los archivos fuente de ambas familias. | `sync_execution_factory` crea dispatcher, tracker y coordinador sincronos. `async_execution_factory` crea sus equivalentes con cola, workers y cierre coordinado. |
| **Factory Method** | `event_line_reader`, `event_file_creator`, `txt_event_creator` y `csv_event_creator` en `include/event_reader.h`; lectores concretos en `src/event_reader.cpp`. | `process_input` en `src/event_processor.cpp` obtiene un creador del registro y llama a `import_file` para cada archivo; `CMakeLists.txt` incorpora `src/event_reader.cpp` y `src/event_creator_registry.cpp`. | `import_file` mantiene el flujo comun de lectura y validacion, y llama a `create_reader()`, el Factory Method que crea el lector concreto. TXT separa por `|`; CSV separa por coma y admite comas entrecomilladas. Ambos validan con `event_builder`. |
| **Factory Registry** | `event_creator_registry` en `include/event_creator_registry.h` y `src/event_creator_registry.cpp`; expone `instance()` como Singleton. | `event_creator_registry::instance()` conserva el registro compartido y `process_input` selecciona `txt` por defecto. | Un `std::unordered_map` asocia cada clave con una funcion que crea su `event_file_creator`. La seleccion depende de una clave, sin condicionales por formato en el flujo de importacion. |

### Objetivo de cada patron en el problema

| Patron | Objetivo en el procesador de eventos | Clase creada o adaptada |
| --- | --- | --- |
| Singleton | Compartir un solo logger para que todos los componentes registren eventos en el mismo archivo de forma segura entre workers. | `event_logger` (adaptada para obtenerse mediante `instance()`). |
| Builder | Construir cada evento a partir de sus campos y evitar entregar objetos incompletos o invalidos al procesamiento. | `event_builder` (nueva); `parse_event` delega en ella. |
| Abstract Factory | Seleccionar una familia compatible de despacho, seguimiento y cierre segun el modo sincrono o asincrono. | `execution_factory` y sus fabricas concretas `sync_execution_factory` y `async_execution_factory`; crean `event_dispatcher`, `result_tracker` y `shutdown_coordinator`. |
| Factory Method | Mantener comun el flujo de importacion y permitir que cada creador elija el lector concreto por formato. | `event_file_creator` define el flujo y `create_reader()`; `txt_event_creator` y `csv_event_creator` son creadores concretos. |
| Factory Registry | Desacoplar la seleccion del creador concreto mediante una clave de formato. | `event_creator_registry` registra las funciones de creacion por clave y devuelve el creador solicitado. |

El punto de entrada del registro es `process_input()` en `src/event_processor.cpp`, que obtiene `event_creator_registry::instance()`. El Singleton registra las claves `txt` y `csv` en su constructor; `input_format` selecciona `txt` por defecto. Para usar CSV, cambia esa clave a `csv`. Para agregar otro formato, implementa su `event_file_creator` y `event_line_reader`, y registra su clave con `register_creator()`. El registro usa `std::unordered_map<std::string, std::function<...>>`; una clave desconocida produce `std::invalid_argument`. Despues de seleccionar el creador, `import_file()` invoca `create_reader()` para construir el lector concreto. El lector CSV espera cuatro campos separados por coma (`tipo,severidad,destinatario,mensaje`) y permite campos entre comillas. Factory Method y Factory Registry trabajan juntos aqui: el registro selecciona la familia de creador y el Factory Method de ese creador construye el lector.

## Compilar y ejecutar

Ejecuta los comandos desde la raiz del proyecto, donde se encuentran `config/` y `entrada/`:

```sh
cmake -S . -B build
cmake --build build
./build/prog3_procesador_eventos entrada sync
./build/prog3_procesador_eventos entrada async 4
```

En Windows, el ejecutable suele estar en `build\\prog3_procesador_eventos.exe`; con un generador multiconfiguracion puede estar en `build\\Debug\\`. Si no se indica una carpeta, se usa `entrada` en modo sincrono.

La aplicacion recorre al iniciar la carpeta indicada y procesa sus archivos `.txt` en orden alfabetico. Cada linea no vacia usa el formato `tipo|severidad|destinatario|mensaje`. Los destinatarios se cargan desde `config/recipients.txt`. Las entregas se escriben en `salida/destinatarios/`, los fallos en `salida/dlq/` y el log central en `logs/events.log`.

## Estructura de codigo

- `include/`: declaraciones de eventos, opciones, configuracion, logger, estadisticas, procesamiento y fabricas de ejecucion.
- `src/`: implementaciones y punto de entrada `main.cpp`.
- `config/recipients.txt`: relacion entre destinatarios logicos y carpetas de salida.
- `entrada/`: archivos TXT de entrada.
- `salida/` y `logs/`: resultados generados durante la ejecucion.

El modo asincrono usa `std::jthread`, una cola protegida con mutex y `condition_variable`. El coordinador cierra la admision, espera a los workers y permite emitir el resumen final.
