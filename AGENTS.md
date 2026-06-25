# AudioFlow3

## Project Type
Qt6 Desktop Application (C++ with CMake)

## Key Files
- `CMakeLists.txt` - Build configuration
- `main.cpp` - Application entry point
- `mainwindow.cpp/h` - Main window implementation
- `mainwindow.ui` - Qt Designer UI file
- `AudioFlow3_en_001.ts` - English translation source

## Build Commands
```bash
# Build (from project root)
cmake -B build -S .
cmake --build build

# Run
./build/AudioFlow3
```

## Architecture
- Single-window Qt Widgets application
- Uses Qt's translation system (i18n)
- UI defined in `mainwindow.ui` (Qt Designer format)

## Important Notes
- Build directory should be separate from source (out-of-source build)
- Generated files (moc, ui, qrc) are auto-handled by CMake - do not edit manually
- Translation files (.ts) are updated via `make update_translations`
- No tests defined yet
