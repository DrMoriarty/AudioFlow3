#ifndef EQ_CPP_READIRFILE_H
#define EQ_CPP_READIRFILE_H

#include <vector>
#include <cstdint>

struct IRData {
    std::vector<float> audioDataL;
    std::vector<float> audioDataR;
    uint32_t sampleRate;
    uint16_t numChannels = 0;
};

struct TrueStereo4chData {
    std::vector<float> ch0; // LL
    std::vector<float> ch1; // LR
    std::vector<float> ch2; // RL
    std::vector<float> ch3; // RR
    bool ok = false;
};

void resampleIR(std::vector<float> &buffer, double srcRate, double dstRate);
IRData readIRFile(const std::string &path, uint32_t deviceSampleRate = 0);
TrueStereo4chData read4chWavFile(const std::string& path, uint32_t dstSampleRate = 0);

#endif //EQ_CPP_READIRFILE_H
