# AudioFlow3

## Project Type
Qt6 Desktop Application (C++ with CMake)

## Key Files
- `CMakeLists.txt` - Build configuration
- `main.cpp` - Application entry point
- `mainwindow.cpp/h` - Main window implementation
- `mainwindow.ui` - Qt Designer UI file
- `collapsibleblock.cpp/h` - Collapsible block widget component
- `knobwidget.cpp/h` - Custom knob (rotary) control for fixed dB values
- `AudioFlow3.qrc` - Qt resource file
- `AudioFlow3_en_001.ts` - English translation source

## Build Commands
```bash
# Build (from project root)
cmake -B build -S .
cmake --build build

# Run
./build/AudioFlow3.app/Contents/MacOS/AudioFlow3
```

## Architecture
- Single-window Qt Widgets application
- Fixed width (600px), height adapts to content via `setFixedSize()` + `adjustSize()`
- Main window contains 5 collapsible blocks in vertical layout
- Block 1 ("Correcting") has content: IR combo, Load IR button, Dry/Wet slider, dB knob
- Custom `KnobWidget`: click cycles through fixed dB values, shows current value below knob
- Uses Qt's translation system (i18n)
- UI defined in `mainwindow.ui` (Qt Designer format)

## Important Notes
- Build directory should be separate from source (out-of-source build)
- Generated files (moc, ui, qrc) are auto-handled by CMake - do not edit manually
- Translation files (.ts) are updated via `make update_translations`
- No tests defined yet
