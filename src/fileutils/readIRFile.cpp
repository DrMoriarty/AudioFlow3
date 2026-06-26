//
// Created by Jeremi Campagna on 2024-07-18.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
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

struct WAVHeader {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
};

struct Chunk {
    char chunkID[4];
    uint32_t chunkSize;
    std::vector<char> data;
};

IRData readIRFile(const std::string &path, uint32_t deviceSampleRate) {
    IRData result;
    result.sampleRate = 0;

    std::string absPath = resolvePath(path);

    std::ifstream file(absPath, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << absPath << " from path: " << path  << std::endl;
        return result;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::strncmp(header.chunkID, "RIFF", 4) != 0 || std::strncmp(header.format, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAV file." << std::endl;
        return result;
    }

    std::vector<Chunk> chunks;

    while ((file.tellg() < header.chunkSize + 8) && !file.eof() && !file.fail()) {
        Chunk chunk;
        file.read(reinterpret_cast<char*>(&chunk.chunkID), sizeof(chunk.chunkID));
        file.read(reinterpret_cast<char*>(&chunk.chunkSize), sizeof(chunk.chunkSize));

        chunk.data.resize(chunk.chunkSize);
        file.read(chunk.data.data(), chunk.chunkSize);

        chunks.push_back(chunk);
    }

    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;

    for (const auto& chunk : chunks) {
        if (std::string(chunk.chunkID, 4) == "fmt ") {
            if (chunk.chunkSize >= 16) {
                audioFormat = *reinterpret_cast<const uint16_t*>(chunk.data.data());
                numChannels = *reinterpret_cast<const uint16_t*>(chunk.data.data() + 2);
                sampleRate = *reinterpret_cast<const uint32_t*>(chunk.data.data() + 4);
                bitsPerSample = *reinterpret_cast<const uint16_t*>(chunk.data.data() + 14);

                bool isPCM = (audioFormat == 1);
                bool isFloat = (audioFormat == 3);
                bool validBits = (bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32);

                if ((numChannels < 1 || numChannels > 2) || (!isPCM && !isFloat) || !validBits) {
                    std::cerr << "Only mono/stereo WAV files (16/24/32-bit PCM or 32-bit float) are supported." << std::endl;
                    return result;
                }

                result.sampleRate = sampleRate;
                result.numChannels = numChannels;
            } else {
                std::cerr << "Unexpected fmt chunk size: " << chunk.chunkSize << std::endl;
                return result;
            }
        }

        if (std::string(chunk.chunkID, 4) == "data") {
            const char* data = chunk.data.data();
            size_t dataSize = chunk.data.size();

            if (bitsPerSample == 16) {
                for (size_t i = 0; i + 1 < dataSize; i += 2) {
                    int16_t sample;
                    std::memcpy(&sample, data + i, 2);
                    result.audioData.push_back(sample / 32768.0f);
                }
            } else if (bitsPerSample == 24) {
                for (size_t i = 0; i + 2 < dataSize; i += 3) {
                    int32_t sample = static_cast<unsigned char>(data[i])
                                  | (static_cast<unsigned char>(data[i + 1]) << 8)
                                  | (static_cast<unsigned char>(data[i + 2]) << 16);
                    if (sample & 0x800000) {
                        sample |= 0xFF000000;
                    }
                    result.audioData.push_back(sample / 8388608.0f);
                }
            } else if (bitsPerSample == 32) {
                if (audioFormat == 3) {
                    for (size_t i = 0; i + 3 < dataSize; i += 4) {
                        float sample;
                        std::memcpy(&sample, data + i, 4);
                        result.audioData.push_back(sample);
                    }
                } else {
                    for (size_t i = 0; i + 3 < dataSize; i += 4) {
                        int32_t sample;
                        std::memcpy(&sample, data + i, 4);
                        result.audioData.push_back(sample / 2147483648.0f);
                    }
                }
            } else {
                std::cerr << "Unsupported bits per sample: " << bitsPerSample << std::endl;
            }

            auto deinterleave = [&](std::vector<float>& L, std::vector<float>& R) {
                if (numChannels == 2) {
                    size_t numFrames = result.audioData.size() / 2;
                    L.reserve(numFrames);
                    R.reserve(numFrames);
                    for (size_t i = 0; i < numFrames; ++i) {
                        L.push_back(result.audioData[2 * i]);
                        R.push_back(result.audioData[2 * i + 1]);
                    }
                } else {
                    L = result.audioData;
                    R = result.audioData;
                }
            };

            deinterleave(result.audioDataL, result.audioDataR);

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
                    float normFactor = static_cast<float>(1.0 / sqrt(sumSq));
                    for (auto& s : v) s *= normFactor;
                }
            };
            normalizeL2(result.audioDataL);
            normalizeL2(result.audioDataR);

        }
    }

    file.close();

    return result;
}
