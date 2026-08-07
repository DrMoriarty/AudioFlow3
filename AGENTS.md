# AudioFlow3

## Project Type
Qt6 Desktop Application (C++ with CMake)

## Key Files
- `CMakeLists.txt` - Build configuration (links Qt6, CoreAudio, Accelerate, libsndfile, libwavpack)
- `main.cpp` - Application entry point
- `mainwindow.cpp/h` - Main window implementation
- `mainwindow.ui` - Qt Designer UI file
- `collapsibleblock.cpp/h` - Collapsible block widget component
- `knobwidget.cpp/h` - Custom knob (rotary) control for fixed dB values
- `toggleswitch.cpp/h` - Toggle switch widget
- `azimuthselector.cpp/h` - Azimuth angle selector widget (arc UI with mouse drag, wheel, animation)
- `correctiongraph.cpp/h` - Correction IR frequency response graph (FFT magnitude, Catmull-Rom spline)
- `equalizergraph.cpp/h` - Equalizer frequency response graph (band summation, Catmull-Rom spline)
- `AudioFlow3.qrc` - Qt resource file
- `AudioFlow3_en_001.ts` - English translation source
- `src/audioflow.cpp/h` - Audio engine (CoreAudio capture + playback via ring buffer)
- `src/permission.mm` - macOS permission request bridge (Objective-C++)
- `src/dockreopen.mm` - macOS dock reopen bridge (Objective-C++)
- `src/processing.cpp/h` - Processing chain coordinator
- `src/processing/audioProcessor.cpp/h` - AudioProcessor wrapper (toggle/mix via Smoother)
- `src/processing/iirFilter.cpp/h` - IIR peak filter (smooth parameter transitions, biquad coefficients)
- `src/processing/binauralRenderer.cpp/h` - BRIR-based binaural rendering (overlap-save FFT convolution, True Stereo mode)
- `src/processing/convolutionReverb.cpp/h` - Generic convolution reverb
- `src/processing/equalizer.cpp/h` - Parametric equalizer (manages array of IIRFilter bands)
- `src/processing/amplifier.cpp/h` - Gain/amplifier stage
- `src/processing/smoother.cpp/h` - Value smoother (linear interpolation over N steps)
- `src/fileutils/readIRFile.cpp/h` - WAV file loading via libsndfile (stereo `readIRFile` and 4ch `read4chWavFile`)
- `src/fileutils/globals.cpp/h` - Global settings and config
- `src/fileutils/config.cpp/h` - Config persistence (JSON)
- `assets/brir/` - BRIR WAV files (35 rooms, 44.1kHz stereo + 4ch True Stereo)

## Build Commands
```bash
# Build (from project root)
cmake -B build -S .
cmake --build build

# Run (from Finder or via open)
open build/AudioFlow3.app
```

## Dependencies
- Qt 6.5+ (build only)
- libsndfile (for WAV file I/O): `brew install libsndfile`
- libwavpack (for WavPack-compressed WAV/IR files): `brew install wavpack`
- CoreAudio, Accelerate (system frameworks)

## Architecture
- Single-window Qt Widgets application
- Fixed width (600px), height adapts to content via `setFixedSize()` + `adjustSize()`
- Signal chain: Correction → Preamplifier → Equalizer → Convolution Reverb → Binaural Renderer
- Each block is a collapsible UI section toggled on/off
- All blocks use separate processing classes coordinated by `processing.cpp`
- Audio runs on a separate thread with lock-free SPSC ring buffer (65536 samples)
- Processing runs in two phases per audio callback: `process()` → `processBinaural()`

### AzimuthSelector (`azimuthselector.cpp/h`)
- Arc-based azimuth angle selector widget (150x150px)
- Mouse drag, scroll wheel, and programmatic angle setting
- Animation on release (snap to nearest integer degree)
- Emits `angleChanged(double)` signal
- Source positions computed from angle via sine/cosine

### CorrectionGraph (`correctiongraph.cpp/h`)
- Displays correction IR frequency response (20–20000 Hz)
- FFT-based magnitude analysis (Accelerate vDSP) with 100 display points on log-frequency scale
- Catmull-Rom spline smoothing of correction and original curves
- Dry/wet mixing visualization with dB scale (-24 dB to 0 dB)
- `setIRData()` takes stereo L/R IR data and sample rate

### EqualizerGraph (`equalizergraph.cpp/h`)
- Displays parametric equalizer frequency response
- Band summation with Gaussian influence kernel (Q-factor controls bandwidth)
- Catmull-Rom spline smoothing of the summed curve
- Individual control points drawn as yellow dots
- `setFrequencyData()`, `setGainData()`, `setQData()` for per-band updates

### AudioProcessor (`src/processing/audioProcessor.cpp/h`)
- Wrapper around `Smoother` for toggle/mix control
- `setToggle(bool)` sets mix to 0.0 or 1.0
- `setMix(double)` smoothly interpolates between current and target mix value
- `process()` delegates to `Smoother` for smooth transitions

### Binaural Renderer Architecture
- `BinauralRenderer` owns the overlap-save FFT convolution engine (`convolutionChunkSize=1024`, padded 2048, 1024 bins)
- Loads BRIR WAV files via `libsndfile` (`readIRFile` for 2ch, `read4chWavFile` for 4ch True Stereo)
- 8 FFT banks: `{NegL,NegR,PosL,PosR,CrossNL,CrossNR,CrossPL,CrossPR}`; `m_fftIn` is shared across convolutions
- True Stereo mode: 4ch WAV with channel layout LL,LR,RL,RR → computes `outL = Conv(inL,LL) + Conv(inR,RL)`, `outR = Conv(inL,LR) + Conv(inR,RR)`
- Drains via `m_drainL/R` with `m_accumL/R` accumulation buffer
- Anti-click crossfade on IR swap: 3-phase envelope (fade-out → gap → fade-in), each phase = `m_chunkSize * 2` ≈ 46ms
- Crossfade state held in `m_xfade{Remaining, PauseRemaining, InRemaining, DrainFadePos, Active}`
- All setters (`setDir`, `setConfig`, `setAngle`, `setElevation`, `setTrueStereo`, `setTargetDuration`) hold `m_mutex` and call `beginCrossfade()` + `loadBRIRUnlocked()`
- BRIR files stored in `assets/brir/` → copied to `.app/Contents/Resources/brir/` at build time
- Room metadata from `assets/brir/Rooms.csv`

### ReadIRFile (`src/fileutils/readIRFile.cpp`)
- `readIRFile(path, deviceSampleRate)` → loads stereo WAV via `libsndfile`, returns `IRData` with deinterleaved L/R and normalized L2
- `read4chWavFile(path, dstSampleRate)` → loads 4ch WAV via `libsndfile`, returns `TrueStereo4chData` with ch0-ch3 deinterleaved
- WavPack fallback via `tryReadWavpack()` (in same file): if `sf_open()` fails (e.g. `.wav` file actually wvpk-encoded), falls back to `libwavpack` decoder
- `assets/ir/*.wav` files are WavPack-compressed (2ch 16-bit 48kHz) — cannot be read by `libsndfile`, handled by `libwavpack`
- Both loaders normalise int32 output from `WavpackUnpackSamples` to float using `1/(2^(bits-1))` scale factor
- Both loaders perform linear-interpolation resampling when file sample rate ≠ device sample rate
- Path resolution: absolute paths passed through; relative paths resolved via `getExeDir()`

## Important Notes
- Build directory should be separate from source (out-of-source build)
- Generated files (moc, ui, qrc) are auto-handled by CMake - do not edit manually
- Translation files (.ts) are updated via `make update_translations`
- libsndfile found via CMake config at `/usr/local/Cellar/libsndfile/` (Homebrew)
- libwavpack found via `find_library` at `/usr/local/Cellar/wavpack/` (Homebrew)
- macOS deployment target: 13.0+
- App runs from Finder (not from terminal) — stderr is invisible; use file-based logging for diagnostics
- No tests defined yet
