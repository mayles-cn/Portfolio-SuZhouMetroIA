# SuZhouMetroIA

Minimal Qt Widgets bootstrap for the SuZhouMetroIA project.

## Project layout

```text
.
|- CMakeLists.txt
|- include/
|- resources/
`- src/
   |- CMakeLists.txt
   |- main.cpp
   |- mainwindow.h/.cpp
   |- models/
   |- services/
   |- utils/
   `- widgets/
```

## Requirements

- CMake 3.21+
- C++17 compiler (MSVC, Clang, or GCC)
- Qt 6 with `Widgets` module

## Build

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
```

Replace `CMAKE_PREFIX_PATH` with your local Qt installation path when needed.

## Current bootstrap scope

- Root and modular CMake build files
- Minimal application entry and main window
- Basic layered placeholders:
  - `models`: metro line model
  - `services`: route demo service
  - `utils`: build info helper
  - `widgets`: reusable info panel
