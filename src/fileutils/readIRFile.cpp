#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.h>
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

IRData readIRFile(const std::string &path, uint32_t deviceSampleRate) {
    IRData result;
    result.sampleRate = 0;

    std::string absPath = resolvePath(path);

    SF_INFO info = {};
    SNDFILE* file = sf_open(absPath.c_str(), SFM_READ, &info);
    if (!file) {
        std::cerr << "Cannot open file: " << absPath << " (" << sf_strerror(nullptr) << ")" << std::endl;
        return result;
    }

    size_t totalSamples = static_cast<size_t>(info.frames) * info.channels;
    std::vector<float> interleaved(totalSamples);
    sf_readf_float(file, interleaved.data(), info.frames);
    sf_close(file);

    result.sampleRate = info.samplerate;
    result.numChannels = info.channels;

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
    std::string absPath = resolvePath(path);

    SF_INFO info = {};
    SNDFILE* file = sf_open(absPath.c_str(), SFM_READ, &info);
    if (!file) {
        std::cerr << "read4chWavFile: cannot open " << absPath << " (" << sf_strerror(nullptr) << ")" << std::endl;
        return result;
    }

    if (info.channels != 4) {
        std::cerr << "read4chWavFile: expected 4 channels, got " << info.channels << std::endl;
        sf_close(file);
        return result;
    }

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
        double srcRate = static_cast<double>(info.samplerate);
        double dstRate = static_cast<double>(dstSampleRate);
        resampleIR(result.ch0, srcRate, dstRate);
        resampleIR(result.ch1, srcRate, dstRate);
        resampleIR(result.ch2, srcRate, dstRate);
        resampleIR(result.ch3, srcRate, dstRate);
    }

    result.ok = true;
    return result;
}
