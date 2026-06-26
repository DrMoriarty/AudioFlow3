//
// Convolution reverb processor — overlap-save FFT implementation
//
// Created by Jeremi Campagna on 2024-07-17.
//

#ifndef EQ_CPP_CONVOLUTIONREVERB_H
#define EQ_CPP_CONVOLUTIONREVERB_H

#include <vector>
#include <string>
#include <Accelerate/Accelerate.h>
#include "smoother.h"
#include "audioProcessor.h"
#include "../fileutils/readIRFile.h"
#include "../fileutils/globals.h"


class ConvolutionReverb : public AudioProcessor {
public:
    std::string path;

    ConvolutionReverb(bool toggle, std::string path, double dryWet, float deviceSampleRate, float postGainDb = 0.0f);
    ~ConvolutionReverb();

    double getDryWet();
    void setDryWet(double newDryWet);
    void setPostGain(float postGainDb);
    float getPostGain();
    float getDeviceSampleRate() const;

    void process(std::vector<float>& input);
private:
    size_t chunkSize;
    size_t paddedSize;
    size_t numBins;
    float deviceSampleRate;
    Smoother dryWet;
    Smoother postGain;
    static constexpr float LN10_OVER_20 = 0.11512925464970229f;

    FFTSetup fftSetup;

    struct SplitComplex {
        std::vector<float> real;
        std::vector<float> imag;
        DSPSplitComplex dsp() { return { real.data(), imag.data() }; }
    };

    std::vector<SplitComplex> irFFTsL;
    std::vector<SplitComplex> irFFTsR;
    std::vector<float> overlapL;
    std::vector<float> overlapR;

    SplitComplex fftReal;
    std::vector<float> inputPadded;

    SplitComplex workerRealL;
    SplitComplex workerRealR;
    std::vector<float> iblittedL;
    std::vector<float> iblittedR;
    std::vector<float> overlapReverbL;
    std::vector<float> overlapReverbR;

    size_t numChunks;

    SplitComplex ifftTmp;

    std::vector<float> accumL;
    std::vector<float> accumR;
    std::vector<float> drainL;
    std::vector<float> drainR;

    std::vector<float> inputL;
    std::vector<float> inputR;
    std::vector<float> outputL;
    std::vector<float> outputR;

    void fftZ(const std::vector<float>& input, float* outReal, float* outImag);
    void ifftZ(SplitComplex& workerReal, std::vector<float>& iblitted);

    void convolveChannel(const std::vector<float>& input, std::vector<float>& output,
                         const std::vector<SplitComplex>& irFFTs, std::vector<float>& overlap,
                         SplitComplex& workerReal, std::vector<float>& iblitted,
                         std::vector<float>& overlapReverb);
};


#endif //EQ_CPP_CONVOLUTIONREVERB_H
