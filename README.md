# Procesador de eventos: baseline sin patrones

Implementación inicial del enunciado en C++20. Usa funciones y objetos ordinarios; todavía no aplica Singleton, Builder, Factory Method ni Abstract Factory. La idea es conservar este comportamiento al introducir cada patrón en etapas posteriores.

El código se divide en `include/` (`.h`) y `src/` (`.cpp`). Cada módulo mantiene una responsabilidad concreta y usa `snake_case` para sus identificadores.

El código se divide en `include/` (`.h`) y `src/` (`.cpp`). Cada módulo mantiene una responsabilidad concreta y usa `snake_case` para sus identificadores.

## Compilar y ejecutar

Ejecuta los comandos desde la raíz del proyecto, donde se encuentran `config/` y `entrada/`:

```sh
cmake -S . -B build
cmake --build build
./build/prog3_procesador_eventos entrada/messages_03.txt sync
./build/prog3_procesador_eventos entrada/messages_03.txt async 4
```

En Windows, el ejecutable suele estar en `build\prog3_procesador_eventos.exe`; con un generador multiconfiguración puede estar en `build\Debug\`. Si no se especifica un archivo, se usa `entrada/messages_01.txt` en modo síncrono.

El programa lee `tipo|severidad|destinatario|mensaje`, valida cada línea y continúa después de los errores. Los destinatarios y sus carpetas se cargan desde `config/recipients.txt`. Las entregas se guardan en `salida/destinatarios/<destinatario>/`, los fallos en `salida/dlq/` y el registro central en `logs/events.log`.

## Partes para las siguientes aproximaciones

- `parseEvent` reúne y valida los campos directamente; puede sustituirse por un Builder.
- `EventLogger` es un objeto creado en `main` y pasado a las funciones; puede evolucionar a Singleton.
- La lectura está implementada directamente con `std::ifstream`; al añadir CSV, se puede introducir Factory Method para seleccionar lectores.
- La selección `sync`/`async` está en `main`; el despacho, los workers y el seguimiento pueden reorganizarse con Abstract Factory.

El modo asíncrono usa una cola protegida con mutex y `condition_variable`, workers `std::jthread` y contadores atómicos. El resumen se emite después de que todos los workers terminan.
