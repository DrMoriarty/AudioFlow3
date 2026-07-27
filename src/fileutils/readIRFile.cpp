#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.h>
#include <wavpack/wavpack.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
#include "readIRFile.h"
#include "../fileutils/globals.h"

static std::string getExeDir() {
    static std::string cached;
    if (!cached.empty()) return cached;
    CFURLRef url = CFBundleCopyExecutableURL(CFBundleGetMainBundle());
    if (url) {
        CFURLRef dir = CFURLCreateCopyDeletingLastPathComponent(nullptr, url);
        char buf[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(dir, true, reinterpret_cast<UInt8*>(buf), PATH_MAX)) {
            cached = buf;
        }
        if (dir) CFRelease(dir);
        CFRelease(url);
    }
    return cached;
}

static std::string resolvePath(const std::string& path) {
    if (path.empty() || path[0] == '/') return path;
    struct stat s;
    if (stat(path.c_str(), &s) == 0) return path;
    std::string exeDir = getExeDir();
    if (exeDir.empty()) return path;
    std::string resolved = exeDir + "/" + path;
    if (stat(resolved.c_str(), &s) == 0) return resolved;
    return path;
}

void resampleIR(std::vector<float> &buffer, double srcRate, double dstRate) {
    if (srcRate == dstRate || buffer.empty() || srcRate <= 0.0 || dstRate <= 0.0) return;
    double ratio = dstRate / srcRate;
    size_t outLen = static_cast<size_t>(std::round(buffer.size() * ratio));
    std::vector<float> out(outLen, 0.0f);
    for (size_t i = 0; i < outLen; ++i) {
        double srcPos = static_cast<double>(i) / ratio;
        size_t idx0 = static_cast<size_t>(srcPos);
        double frac = srcPos - static_cast<double>(idx0);
        if (idx0 + 1 < buffer.size()) {
            out[i] = static_cast<float>(buffer[idx0] * (1.0 - frac) + buffer[idx0 + 1] * frac);
        } else if (idx0 < buffer.size()) {
            out[i] = buffer[idx0];
        }
    }
    buffer = std::move(out);
}

struct WavpackDeinterleaved {
    std::vector<std::vector<float>> channels;
    uint32_t sampleRate = 0;
    uint16_t numChannels = 0;
    bool ok = false;
};

static bool tryReadWavpack(const std::string& absPath, WavpackDeinterleaved& out) {
    char err[256] = {};
    WavpackContext* wpc = WavpackOpenFileInput(absPath.c_str(), err,
                            OPEN_WRAPPER | OPEN_NORMALIZE, 0);
    if (!wpc) {
        std::cerr << "wavpack: cannot open " << absPath << ": " << err << std::endl;
        return false;
    }

    uint32_t numSamples = WavpackGetNumSamples(wpc);
    int sr             = WavpackGetSampleRate(wpc);
    int numCh          = WavpackGetNumChannels(wpc);
    int bps            = WavpackGetBitsPerSample(wpc);

    if (numSamples == 0 || numSamples == (uint32_t)-1 || numCh < 1 || numCh > 8) {
        WavpackCloseFile(wpc);
        return false;
    }

    out.numChannels = static_cast<uint16_t>(numCh);
    out.sampleRate  = static_cast<uint32_t>(sr);
    out.channels.resize(numCh);

    size_t frames = static_cast<size_t>(numSamples);
    std::vector<int32_t> raw(frames * static_cast<size_t>(numCh));
    uint32_t got = WavpackUnpackSamples(wpc, raw.data(), numSamples);
    WavpackCloseFile(wpc);

    if (got == 0) return false;

    float scale = 1.0f / static_cast<float>(1L << (bps - 1));

    for (int c = 0; c < numCh; ++c) {
        out.channels[c].resize(got);
        for (size_t i = 0; i < got; ++i)
            out.channels[c][i] = static_cast<float>(raw[i * numCh + c]) * scale;
    }

    out.ok = true;
    return true;
}

IRData readIRFile(const std::string &path, uint32_t deviceSampleRate) {
    IRData result;
    result.sampleRate = 0;

    std::string absPath = resolvePath(path);

    // ── try libsndfile first ──────────────────────────────────────────
    SF_INFO info = {};
    SNDFILE* file = sf_open(absPath.c_str(), SFM_READ, &info);
    if (file) {
        size_t totalSamples = static_cast<size_t>(info.frames) * info.channels;
        std::vector<float> interleaved(totalSamples);
        sf_readf_float(file, interleaved.data(), info.frames);
        sf_close(file);

        result.sampleRate    = static_cast<uint32_t>(info.samplerate);
        result.numChannels   = static_cast<uint16_t>(info.channels);

        if (info.channels == 2) {
            size_t numFrames = static_cast<size_t>(info.frames);
            result.audioDataL.reserve(numFrames);
            result.audioDataR.reserve(numFrames);
            for (size_t i = 0; i < numFrames; ++i) {
                result.audioDataL.push_back(interleaved[2 * i]);
                result.audioDataR.push_back(interleaved[2 * i + 1]);
            }
        } else {
            result.audioDataL = interleaved;
            result.audioDataR = interleaved;
        }
    } else if (WavpackDeinterleaved wp; tryReadWavpack(absPath, wp)) {
        // ── fallback: libwavpack ──────────────────────────────────────
        result.sampleRate  = wp.sampleRate;
        result.numChannels = wp.numChannels;

        if (wp.numChannels >= 2) {
            result.audioDataL = std::move(wp.channels[0]);
            result.audioDataR = std::move(wp.channels[1]);
        } else {
            result.audioDataL = wp.channels[0];
            result.audioDataR = wp.channels[0];
        }
    } else {
        std::cerr << "Cannot open file: " << absPath << std::endl;
        return result;
    }

    // ── resample if needed ────────────────────────────────────────────
    if (deviceSampleRate > 0 && result.sampleRate > 0 &&
        deviceSampleRate != result.sampleRate) {
        resampleIR(result.audioDataL, static_cast<double>(result.sampleRate),
                   static_cast<double>(deviceSampleRate));
        resampleIR(result.audioDataR, static_cast<double>(result.sampleRate),
                   static_cast<double>(deviceSampleRate));
    }

    if (result.audioDataL.size() < convolutionChunkSize) {
        result.audioDataL.resize(convolutionChunkSize, 0.0f);
        result.audioDataR.resize(convolutionChunkSize, 0.0f);
    }

    auto normalizeL2 = [](std::vector<float>& v) {
        double sumSq = 0.0;
        for (float s : v) sumSq += static_cast<double>(s) * s;
        if (sumSq > 0.0) {
            float normFactor = static_cast<float>(1.0 / std::sqrt(sumSq));
            for (auto& s : v) s *= normFactor;
        }
    };
    normalizeL2(result.audioDataL);
    normalizeL2(result.audioDataR);

    return result;
}

TrueStereo4chData read4chWavFile(const std::string& path, uint32_t dstSampleRate) {
    TrueStereo4chData result;
    if (path.empty()) return result;

    std::string absPath = resolvePath(path);

    // ── try libsndfile first ──────────────────────────────────────────
    {
        SF_INFO info = {};
        SNDFILE* file = sf_open(absPath.c_str(), SFM_READ, &info);
        if (file && info.channels == 4) {
            size_t numFrames = static_cast<size_t>(info.frames);
            std::vector<float> interleaved(numFrames * 4);
            sf_readf_float(file, interleaved.data(), info.frames);
            sf_close(file);

            result.ch0.resize(numFrames);
            result.ch1.resize(numFrames);
            result.ch2.resize(numFrames);
            result.ch3.resize(numFrames);
            for (size_t i = 0; i < numFrames; ++i) {
                size_t j = i * 4;
                result.ch0[i] = interleaved[j];
                result.ch1[i] = interleaved[j + 1];
                result.ch2[i] = interleaved[j + 2];
                result.ch3[i] = interleaved[j + 3];
            }

            if (dstSampleRate > 0 && info.samplerate > 0 &&
                static_cast<uint32_t>(info.samplerate) != dstSampleRate) {
                double srcR = static_cast<double>(info.samplerate);
                double dstR = static_cast<double>(dstSampleRate);
                resampleIR(result.ch0, srcR, dstR);
                resampleIR(result.ch1, srcR, dstR);
                resampleIR(result.ch2, srcR, dstR);
                resampleIR(result.ch3, srcR, dstR);
            }
            result.ok = true;
            return result;
        }
        if (file) sf_close(file);
    }

    // ── fallback: libwavpack ─────────────────────────────────────────
    WavpackDeinterleaved wp;
    if (tryReadWavpack(absPath, wp) && wp.numChannels >= 4) {
        result.ch0 = std::move(wp.channels[0]);
        result.ch1 = std::move(wp.channels[1]);
        result.ch2 = std::move(wp.channels[2]);
        result.ch3 = std::move(wp.channels[3]);

        if (dstSampleRate > 0 && wp.sampleRate > 0 &&
            wp.sampleRate != dstSampleRate) {
            double srcR = static_cast<double>(wp.sampleRate);
            double dstR = static_cast<double>(dstSampleRate);
            resampleIR(result.ch0, srcR, dstR);
            resampleIR(result.ch1, srcR, dstR);
            resampleIR(result.ch2, srcR, dstR);
            resampleIR(result.ch3, srcR, dstR);
        }
        result.ok = true;
    } else {
        std::cerr << "read4chWavFile: cannot read " << absPath << std::endl;
    }
    return result;
}
