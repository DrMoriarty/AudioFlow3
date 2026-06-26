//
// Created by Jeremi Campagna on 2024-07-17.
//

#include "convolutionReverb.h"
#include <iostream>

ConvolutionReverb::ConvolutionReverb(bool toggle, std::string path, double dryWet, float deviceSampleRate, float postGainDb)
    : AudioProcessor(toggle), path(path), dryWet(Smoother(dryWet, dryWet, 0)), postGain(Smoother(postGainDb, postGainDb, 0)), deviceSampleRate(deviceSampleRate) {

    chunkSize = convolutionChunkSize;
    paddedSize = chunkSize * 2;
    numBins = paddedSize / 2;
    numChunks = 0;

    IRData irData;
    irData.sampleRate = 0;

    fftSetup = vDSP_create_fftsetup(static_cast<vDSP_Length>(std::log2(paddedSize)), FFT_RADIX2);

    if (!path.empty()) {
        irData = readIRFile(path, static_cast<uint32_t>(deviceSampleRate));
    }

    if (!irData.audioDataL.empty() && !irData.audioDataR.empty()) {
        numChunks = static_cast<size_t>(std::ceil(static_cast<float>(irData.audioDataL.size()) / chunkSize));

        irData.audioDataL.resize(numChunks * chunkSize, 0.0f);
        irData.audioDataR.resize(numChunks * chunkSize, 0.0f);

        SplitComplex fftTmp{std::vector<float>(numBins), std::vector<float>(numBins)};
        std::vector<float> chunkBuf(paddedSize, 0.0f);

        for (size_t i = 0; i < numChunks; ++i) {
            std::copy(irData.audioDataL.begin() + i * chunkSize,
                      irData.audioDataL.begin() + (i + 1) * chunkSize,
                      chunkBuf.begin());
            std::fill(chunkBuf.begin() + chunkSize, chunkBuf.end(), 0.0f);
            fftZ(chunkBuf, fftTmp.real.data(), fftTmp.imag.data());
            SplitComplex dstL{std::vector<float>(numBins), std::vector<float>(numBins)};
            std::copy(fftTmp.real.begin(), fftTmp.real.end(), dstL.real.begin());
            std::copy(fftTmp.imag.begin(), fftTmp.imag.end(), dstL.imag.begin());
            irFFTsL.push_back(std::move(dstL));

            std::copy(irData.audioDataR.begin() + i * chunkSize,
                      irData.audioDataR.begin() + (i + 1) * chunkSize,
                      chunkBuf.begin());
            std::fill(chunkBuf.begin() + chunkSize, chunkBuf.end(), 0.0f);
            fftZ(chunkBuf, fftTmp.real.data(), fftTmp.imag.data());
            SplitComplex dstR{std::vector<float>(numBins), std::vector<float>(numBins)};
            std::copy(fftTmp.real.begin(), fftTmp.real.end(), dstR.real.begin());
            std::copy(fftTmp.imag.begin(), fftTmp.imag.end(), dstR.imag.begin());
            irFFTsR.push_back(std::move(dstR));
        }
    } else {
        std::cerr << "IR has no channel data, reverb disabled." << std::endl;
    }

    size_t totalSize = (numChunks + 2) * chunkSize;
    overlapL.resize(totalSize, 0.0f);
    overlapR.resize(totalSize, 0.0f);

    fftReal = {std::vector<float>(numBins), std::vector<float>(numBins)};
    workerRealL = {std::vector<float>(numBins), std::vector<float>(numBins)};
    workerRealR = {std::vector<float>(numBins), std::vector<float>(numBins)};
    iblittedL.resize(paddedSize, 0.0f);
    iblittedR.resize(paddedSize, 0.0f);
    overlapReverbL.resize(totalSize, 0.0f);
    overlapReverbR.resize(totalSize, 0.0f);

    inputPadded.resize(paddedSize, 0.0f);
    ifftTmp = {std::vector<float>(numBins), std::vector<float>(numBins)};
}

ConvolutionReverb::~ConvolutionReverb() {
    if (fftSetup) {
        vDSP_destroy_fftsetup(fftSetup);
    }
}

void ConvolutionReverb::fftZ(const std::vector<float>& input, float* outReal, float* outImag) {
    if (input.size() < paddedSize) return;
    DSPSplitComplex sc = { outReal, outImag };
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(input.data()), 2, &sc, 1, numBins);
    vDSP_fft_zrip(fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(paddedSize)), FFT_FORWARD);
    float scale = 0.5f;
    vDSP_vsmul(outReal, 1, &scale, outReal, 1, numBins);
    vDSP_vsmul(outImag, 1, &scale, outImag, 1, numBins);
}

void ConvolutionReverb::ifftZ(SplitComplex& workerReal, std::vector<float>& iblitted) {
    std::copy(workerReal.real.begin(), workerReal.real.end(), ifftTmp.real.begin());
    std::copy(workerReal.imag.begin(), workerReal.imag.end(), ifftTmp.imag.begin());
    DSPSplitComplex sc = ifftTmp.dsp();
    vDSP_fft_zrip(fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(paddedSize)), FFT_INVERSE);
    vDSP_ztoc(&sc, 1, reinterpret_cast<COMPLEX*>(iblitted.data()), 2, numBins);
    const float factor = 1.0f / static_cast<float>(paddedSize);
    vDSP_vsmul(iblitted.data(), 1, &factor, iblitted.data(), 1, paddedSize);
}

void ConvolutionReverb::convolveChannel(const std::vector<float>& input, std::vector<float>& output,
                                         const std::vector<SplitComplex>& irFFTs, std::vector<float>& overlap,
                                         SplitComplex& workerReal, std::vector<float>& iblitted,
                                         std::vector<float>& overlapReverb) {
    const size_t inputSize = input.size();
    const size_t totalSize = overlap.size();

    std::copy(input.begin(), input.end(), inputPadded.begin());
    fftZ(inputPadded, fftReal.real.data(), fftReal.imag.data());

    for (size_t i = 0; i < irFFTs.size(); ++i) {
        DSPSplitComplex inSc = fftReal.dsp();
        DSPSplitComplex irSc = const_cast<SplitComplex&>(irFFTs[i]).dsp();
        DSPSplitComplex outSc = workerReal.dsp();
        vDSP_zvmul(&inSc, 1, &irSc, 1, &outSc, 1, numBins, 1);

        ifftZ(workerReal, iblitted);

        for (size_t k = 0; k < paddedSize; ++k) {
            overlapReverb[i * chunkSize + k] += iblitted[k];
        }
    }

    for (size_t k = 0; k < totalSize; ++k) {
        overlap[k] += overlapReverb[k];
    }

    for (size_t i = 0; i < inputSize; ++i) {
        output[i] = overlap[i];
    }

    std::copy(overlap.begin() + inputSize, overlap.begin() + totalSize, overlap.begin());
    std::fill(overlap.begin() + (totalSize - inputSize), overlap.end(), 0.0f);
}

void ConvolutionReverb::process(std::vector<float>& input) {
    if (mix.currentValueNoChange() <= 0 && mix.getRemaining() <= 0) {
        return;
    }

    if (numChunks == 0) {
        return;
    }

    const size_t inputSize = input.size();
    const size_t numFrames = inputSize / 2;

    for (size_t i = 0; i < numFrames; ++i) {
        accumL.push_back(input[2 * i]);
        accumR.push_back(input[2 * i + 1]);
    }

    inputL.resize(chunkSize);
    inputR.resize(chunkSize);
    outputL.resize(chunkSize);
    outputR.resize(chunkSize);

    while (accumL.size() >= chunkSize) {
        std::copy(accumL.begin(), accumL.begin() + chunkSize, inputL.begin());
        std::copy(accumR.begin(), accumR.begin() + chunkSize, inputR.begin());

        accumL.erase(accumL.begin(), accumL.begin() + chunkSize);
        accumR.erase(accumR.begin(), accumR.begin() + chunkSize);

        std::fill(overlapReverbL.begin(), overlapReverbL.end(), 0.0f);
        std::fill(overlapReverbR.begin(), overlapReverbR.end(), 0.0f);

        convolveChannel(inputL, outputL, irFFTsL, overlapL,
                        workerRealL, iblittedL, overlapReverbL);
        convolveChannel(inputR, outputR, irFFTsR, overlapR,
                        workerRealR, iblittedR, overlapReverbR);

        drainL.insert(drainL.end(), outputL.begin(), outputL.begin() + chunkSize);
        drainR.insert(drainR.end(), outputR.begin(), outputR.begin() + chunkSize);
    }

    if (drainL.size() >= numFrames) {
        bool gainConst = postGain.getRemaining() <= 0;
        float postGainLin = gainConst ? std::exp(LN10_OVER_20 * postGain.currentValueNoChange()) : 0.0f;
        bool gainActive = gainConst ? (postGainLin != 1.0f) : true;
        bool mixConst = dryWet.getRemaining() <= 0 && mix.getRemaining() <= 0;

        if (gainActive) {
            if (mixConst) {
                float scale = static_cast<float>(dryWet.currentValueNoChange() * mix.currentValueNoChange());
                float oneMinusScale = 1.0f - scale;
                for (size_t i = 0; i < numFrames; ++i) {
                    float gL = gainConst ? (drainL[i] * postGainLin) : (drainL[i] * std::exp(LN10_OVER_20 * postGain.currentValue()));
                    float gR = gainConst ? (drainR[i] * postGainLin) : (drainR[i] * std::exp(LN10_OVER_20 * postGain.currentValue()));
                    input[2 * i]     = gL * scale + input[2 * i]     * oneMinusScale;
                    input[2 * i + 1] = gR * scale + input[2 * i + 1] * oneMinusScale;
                }
            } else {
                for (size_t i = 0; i < numFrames; ++i) {
                    float gL = gainConst ? (drainL[i] * postGainLin) : (drainL[i] * std::exp(LN10_OVER_20 * postGain.currentValue()));
                    float gR = gainConst ? (drainR[i] * postGainLin) : (drainR[i] * std::exp(LN10_OVER_20 * postGain.currentValue()));
                    float wetScale = static_cast<float>(dryWet.currentValue() * mix.currentValue());
                    float oneMinusScale = 1.0f - wetScale;
                    input[2 * i]     = gL * wetScale + input[2 * i]     * oneMinusScale;
                    input[2 * i + 1] = gR * wetScale + input[2 * i + 1] * oneMinusScale;
                }
            }
        } else {
            if (mixConst) {
                float scale = static_cast<float>(dryWet.currentValueNoChange() * mix.currentValueNoChange());
                float oneMinusScale = 1.0f - scale;
                for (size_t i = 0; i < numFrames; ++i) {
                    input[2 * i]     = drainL[i] * scale + input[2 * i]     * oneMinusScale;
                    input[2 * i + 1] = drainR[i] * scale + input[2 * i + 1] * oneMinusScale;
                }
            } else {
                for (size_t i = 0; i < numFrames; ++i) {
                    float wetScale = static_cast<float>(dryWet.currentValue() * mix.currentValue());
                    float oneMinusScale = 1.0f - wetScale;
                    input[2 * i]     = drainL[i] * wetScale + input[2 * i]     * oneMinusScale;
                    input[2 * i + 1] = drainR[i] * wetScale + input[2 * i + 1] * oneMinusScale;
                }
            }
        }
        drainL.erase(drainL.begin(), drainL.begin() + numFrames);
        drainR.erase(drainR.begin(), drainR.begin() + numFrames);
    }
}

double ConvolutionReverb::getDryWet() {
    return dryWet.currentValueNoChange();
}

void ConvolutionReverb::setDryWet(double newDryWet) {
    dryWet = Smoother(dryWet.currentValueNoChange(), newDryWet, smootherSteps);
}

void ConvolutionReverb::setPostGain(float postGainDb) {
    postGain = Smoother(postGain.currentValueNoChange(), postGainDb, smootherSteps);
}

float ConvolutionReverb::getPostGain() {
    return postGain.currentValueNoChange();
}

float ConvolutionReverb::getDeviceSampleRate() const {
    return deviceSampleRate;
}
