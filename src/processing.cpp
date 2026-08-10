//
// Created by Jeremi Campagna on 2024-07-16.
//

#include "processing.h"
#include "./fileutils/globals.h"
#include "./processing/clipDetector.h"
#include <thread>
#include <mutex>
#include <cmath>
#include "iostream"


Processing::Processing(const Config& config, double volume, float deviceSampleRate) :
    config(config),
    deviceSampleRate(deviceSampleRate),
    equalizer(std::make_shared<Equalizer>(config.equalizerToggle, config.equalizerF, config.equalizerQ, config.equalizerG, deviceSampleRate)),
    correctionConvolver(std::make_shared<ConvolutionReverb>(config.correctionToggle, config.correctionIRFilePath, config.correctionDryWet, deviceSampleRate, config.correctionPostGain)),
    convolutionReverb(std::make_shared<ConvolutionReverb>(config.reverbToggle, config.irFilePath, config.reverbDryWet, deviceSampleRate)),
    binauralRenderer(std::make_shared<BinauralRenderer>(config.binauralToggle, config.binauralRoom, config.binauralConfig, config.binauralAngle, deviceSampleRate, config.binauralTrueStereo)),
    volume(volume)
{
    m_correctionPreGain.store(config.correctionPreGain, std::memory_order_relaxed);
    m_equalizerPreGain.store(config.equalizerPreGain, std::memory_order_relaxed);
    m_reverbPreGain.store(config.reverbPreGain, std::memory_order_relaxed);
    m_binauralPreGain.store(config.binauralPreGain, std::memory_order_relaxed);
    m_masterGain.store(config.masterGain, std::memory_order_relaxed);
}

Processing::Processing(const Config& config, const Processing* old, double volume, float deviceSampleRate) :
        config(config),
        deviceSampleRate(deviceSampleRate),
        equalizer(std::move(old->equalizer)),
        correctionConvolver(std::move(old->correctionConvolver)),
        convolutionReverb(std::move(old->convolutionReverb)),
        volume(volume)
{
    binauralRenderer = std::make_shared<BinauralRenderer>(config.binauralToggle, config.binauralRoom, config.binauralConfig, config.binauralAngle, deviceSampleRate, config.binauralTrueStereo);

    m_correctionPreGain.store(config.correctionPreGain, std::memory_order_relaxed);
    m_equalizerPreGain.store(config.equalizerPreGain, std::memory_order_relaxed);
    m_reverbPreGain.store(config.reverbPreGain, std::memory_order_relaxed);
    m_binauralPreGain.store(config.binauralPreGain, std::memory_order_relaxed);

    if (equalizer->getToggle() != config.equalizerToggle) {
        equalizer->setToggle(config.equalizerToggle);
    }
    std::vector<IIRFilter>& filters = equalizer->getFilters();
    for (size_t i = 0; i < filters.size(); ++i) {
        if (filters.at(i).getFrequency() != config.equalizerF.at(i)) {
            filters.at(i).setFrequency(config.equalizerF.at(i));
        }
        if (filters.at(i).getQuality() != config.equalizerQ.at(i)) {
            filters.at(i).setQuality(config.equalizerQ.at(i));
        }
        if (filters.at(i).getGain() != config.equalizerG.at(i)) {
            filters.at(i).setGain(config.equalizerG.at(i));
        }
    }

    if (convolutionReverb->getToggle() != config.reverbToggle) {
        convolutionReverb->setToggle(config.reverbToggle);
    }
    if (convolutionReverb->getDryWet() != config.reverbDryWet) {
        convolutionReverb->setDryWet(config.reverbDryWet);
    } else if (convolutionReverb->path != config.irFilePath || convolutionReverb->getDeviceSampleRate() != deviceSampleRate) {
        std::thread ([this, config, deviceSampleRate]() {
            convolutionReverb->setToggle(false);
            auto newConvolutionReverb = std::make_shared<ConvolutionReverb>(config.reverbToggle, config.irFilePath, config.reverbDryWet, deviceSampleRate);

            std::lock_guard<std::mutex> lock(swapMutex);
            convolutionReverb = std::move(newConvolutionReverb);
        }).detach();
    }

    if (correctionConvolver->getToggle() != config.correctionToggle) {
        correctionConvolver->setToggle(config.correctionToggle);
    }
    if (correctionConvolver->getDryWet() != config.correctionDryWet) {
        correctionConvolver->setDryWet(config.correctionDryWet);
    }
    if (correctionConvolver->getPostGain() != config.correctionPostGain) {
        correctionConvolver->setPostGain(config.correctionPostGain);
    }
    if (correctionConvolver->path != config.correctionIRFilePath || correctionConvolver->getDeviceSampleRate() != deviceSampleRate) {
        std::thread ([this, config, deviceSampleRate]() {
            correctionConvolver->setToggle(false);
            auto newCorrectionConvolver = std::make_shared<ConvolutionReverb>(config.correctionToggle, config.correctionIRFilePath, config.correctionDryWet, deviceSampleRate, config.correctionPostGain);

            std::lock_guard<std::mutex> lock(swapMutex);
            correctionConvolver = std::move(newCorrectionConvolver);
        }).detach();
    }
}

static void applyPreGain(std::vector<float>& buf, float db) {
    if (db == 0.0f) return;
    float lin = std::pow(10.0f, db / 20.0f);
    for (auto &s : buf) s *= lin;
}

static void applyPreGain(std::vector<double>& buf, float db) {
    if (db == 0.0f) return;
    double lin = std::pow(10.0, static_cast<double>(db) / 20.0);
    for (auto &s : buf) s *= lin;
}

void Processing::process(std::vector<float>& input) {
    float corrGain = m_correctionPreGain.load(std::memory_order_relaxed);
    if (corrGain != 0.0f)
        applyPreGain(input, corrGain);

    {
        std::lock_guard<std::mutex> lock(swapMutex);
        correctionConvolver->process(input);
    }

    std::vector<double> doubleInput(input.begin(), input.end());

    float eqGain = m_equalizerPreGain.load(std::memory_order_relaxed);
    if (eqGain != 0.0f)
        applyPreGain(doubleInput, eqGain);

    equalizer->process(doubleInput);

    std::vector<float> processed(doubleInput.begin(), doubleInput.end());
    input = processed;

    float revGain = m_reverbPreGain.load(std::memory_order_relaxed);
    if (revGain != 0.0f)
        applyPreGain(input, revGain);

    std::lock_guard<std::mutex> lock(swapMutex);
    convolutionReverb->process(input);
}

void Processing::setReverbToggle(bool toggle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    convolutionReverb->setToggle(toggle);
}

void Processing::setReverbDryWet(double dryWet) {
    std::lock_guard<std::mutex> lock(swapMutex);
    convolutionReverb->setDryWet(dryWet);
}

void Processing::setReverbIRFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(swapMutex);
    auto newConvolutionReverb = std::make_shared<ConvolutionReverb>(convolutionReverb->getToggle(), path, convolutionReverb->getDryWet(), deviceSampleRate);
    convolutionReverb = std::move(newConvolutionReverb);
}

void Processing::setCorrectionToggle(bool toggle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    correctionConvolver->setToggle(toggle);
}

void Processing::setCorrectionDryWet(double dryWet) {
    std::lock_guard<std::mutex> lock(swapMutex);
    correctionConvolver->setDryWet(dryWet);
}

void Processing::setCorrectionIRFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(swapMutex);
    auto newCorrectionConvolver = std::make_shared<ConvolutionReverb>(correctionConvolver->getToggle(), path, correctionConvolver->getDryWet(), deviceSampleRate, correctionConvolver->getPostGain());
    correctionConvolver = std::move(newCorrectionConvolver);
}

void Processing::setCorrectionPostGain(float postGainDb) {
    std::lock_guard<std::mutex> lock(swapMutex);
    correctionConvolver->setPostGain(postGainDb);
}

void Processing::setEqualizerToggle(bool toggle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    equalizer->setToggle(toggle);
}

void Processing::setEqualizerBand(int index, float f, float q, float g) {
    std::lock_guard<std::mutex> lock(swapMutex);
    auto& filters = equalizer->getFilters();
    if (index >= 0 && index < static_cast<int>(filters.size())) {
        filters[index].setFrequency(f);
        filters[index].setQuality(q);
        filters[index].setGain(g);
    }
}

Processing::IRStatus Processing::getCorrectionIRStatus() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(swapMutex));
    bool hasFile = !correctionConvolver->path.empty();
    bool loaded = correctionConvolver->getNumChunks() > 0;
    float duration = correctionConvolver->getIRDurationSec();
    return {hasFile, loaded, duration};
}

Processing::IRStatus Processing::getReverbIRStatus() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(swapMutex));
    bool hasFile = !convolutionReverb->path.empty();
    bool loaded = convolutionReverb->getNumChunks() > 0;
    float duration = convolutionReverb->getIRDurationSec();
    return {hasFile, loaded, duration};
}

void Processing::setBinauralToggle(bool toggle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setToggle(toggle);
}

void Processing::setBinauralDryWet(double dryWet) {
    (void)dryWet;
}

void Processing::setBinauralAngle(int angle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setAngle(angle);
}

void Processing::setBinauralConfig(const std::string& config) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setConfig(config);
}

void Processing::setBinauralRoom(const std::string& room, const std::string& configOverride) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setDir(room, configOverride);
    if (!configOverride.empty()) gConfig->binauralConfig = configOverride;
}

void Processing::setBinauralTrueStereo(bool enabled) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setTrueStereo(enabled);
}


void Processing::setBinauralTargetDuration(float sec) {
    std::lock_guard<std::mutex> lock(swapMutex);
    binauralRenderer->setTargetDuration(sec);
}

void Processing::processBinaural(std::vector<float>& buffer) {
    float bnGain = m_binauralPreGain.load(std::memory_order_relaxed);
    if (bnGain != 0.0f)
        applyPreGain(buffer, bnGain);
    binauralRenderer->processInterleaved(buffer);

    float masterDb = m_masterGain.load(std::memory_order_relaxed);
    if (masterDb != 0.0f) {
        applyPreGain(buffer, masterDb);
    }
    if (detectClipping(buffer))
        m_masterClipCount.fetch_add(1, std::memory_order_relaxed);
}

void Processing::setCorrectionPreGain(float db) {
    m_correctionPreGain.store(db, std::memory_order_relaxed);
}

void Processing::setEqualizerPreGain(float db) {
    m_equalizerPreGain.store(db, std::memory_order_relaxed);
}

void Processing::setReverbPreGain(float db) {
    m_reverbPreGain.store(db, std::memory_order_relaxed);
}

void Processing::setBinauralPreGain(float db) {
    m_binauralPreGain.store(db, std::memory_order_relaxed);
}

int Processing::consumeClipCount_Correction() {
    std::lock_guard<std::mutex> lock(swapMutex);
    return correctionConvolver->consumeClipCount();
}

int Processing::consumeClipCount_Equalizer() {
    std::lock_guard<std::mutex> lock(swapMutex);
    return equalizer->consumeClipCount();
}

int Processing::consumeClipCount_Reverb() {
    std::lock_guard<std::mutex> lock(swapMutex);
    return convolutionReverb->consumeClipCount();
}

int Processing::consumeClipCount_Binaural() {
    std::lock_guard<std::mutex> lock(swapMutex);
    return binauralRenderer->consumeClipCount();
}

void Processing::setMasterGain(float db) {
    m_masterGain.store(db, std::memory_order_relaxed);
}

int Processing::consumeClipCount_Master() {
    return m_masterClipCount.exchange(0, std::memory_order_relaxed);
}
