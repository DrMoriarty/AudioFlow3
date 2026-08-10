# AudioFlow3

## Project Type
macOS-only Qt6 desktop app (C++ with CMake). Uses CoreAudio — **cannot be run from terminal**; launch via `open build/AudioFlow3.app` or Finder. stderr is invisible at runtime; use file-based logging for diagnostics.

## Build Commands
```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build
```
- If Qt6 or libsndfile are not found, set `CMAKE_PREFIX_PATH` to the Homebrew prefix (`brew --prefix qt@6`).
- Version is read from the `version` file (e.g. `3.1.0`); pass to `Info.plist` via CMake `configure_file`.

## Dependencies
- Qt 6.5+ (`brew install qt@6`) — build only
- libsndfile (`brew install libsndfile`) — found via CMake config (SndFile::sndfile)
- libwavpack (`brew install wavpack`) — found via `find_library`
- CoreAudio, Accelerate, AVFoundation, Foundation (system frameworks)
- `src/lib/json.hpp` — vendored nlohmann/json header-only library

## Deployment
```bash
# Ad-hoc release build
macdeployqt build/AudioFlow3.app -verbose=2
codesign --force --deep --sign - build/AudioFlow3.app
# Or use build.sh which does the above + zip with version from `version` file
```

## CI
`.github/workflows/build.yml` — manual trigger only (`workflow_dispatch`), builds arm64 on `macos-latest`, installs Qt via Homebrew.

## Architecture
- Single-window Qt Widgets app; fixed 600px width, height adapts via `setFixedSize()` + `adjustSize()`.
- **Signal chain:** Correction → Preamplifier → Equalizer → Convolution Reverb → Binaural Renderer. Each block is a collapsible section toggled on/off, coordinated by `src/processing.cpp`.
- Audio runs on a **separate thread** with a lock-free SPSC ring buffer (65536 samples). Processing runs in two phases per callback: `process()` → `processBinaural()`.
- **Custom widgets** live at the project root (not in `src/`): `knobwidget`, `toggleswitch`, `azimuthselector`, `correctiongraph`, `equalizergraph`, `collapsibleblock`, `ledindicator`.
- **Processing classes** live in `src/processing/`: AudioProcessor, IIRFilter, Equalizer, Amplifier, ConvolutionReverb, BinauralRenderer, Smoother.
- `assets/brir/` contains 35 rooms (44.1kHz stereo + 4ch True Stereo). Copied to `.app/Contents/Resources/` at build time. Room metadata from `assets/brir/Rooms.csv`.
- `assets/ir/*.wav` are WavPack-compressed (not plain WAV) — `readIRFile` has a `libwavpack` fallback when `libsndfile` fails.

## Binaural Renderer
- Overlap-save FFT convolution (chunk=1024, padded 2048, 1024 bins).
- 8 FFT banks for True Stereo crossfade; `m_fftIn` is shared across convolutions.
- True Stereo mode: 4ch WAV (LL,LR,RL,RR) → `outL = Conv(inL,LL) + Conv(inR,RL)`, etc.
- Anti-click crossfade on IR swap: 3-phase envelope (fade-out → gap → fade-in), each ≈46ms.
- All setters hold `m_mutex` and call `beginCrossfade()` + `loadBRIRUnlocked()`.

## Important Notes
- Generated files (moc, ui, qrc, autogen) are handled by CMake — do not edit manually.
- Translation files (.ts) are updated via `make update_translations` in the build directory.
- macOS deployment target: 13.0+.
- No tests defined yet.
