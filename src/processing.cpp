//
// Created by Jeremi Campagna on 2024-07-16.
//

#include "processing.h"
#include <thread>
#include <mutex>
#include "iostream"


Processing::Processing(const Config& config, double volume, float deviceSampleRate) :
    config(config),
    deviceSampleRate(deviceSampleRate),
    amplifier([&]() {
        auto amp = std::make_shared<Amplifier>(config.ampToggle, config.ampGain);
        amp->setAutoGain(config.ampAuto);
        return amp;
    }()),
    equalizer(std::make_shared<Equalizer>(config.equalizerToggle, config.equalizerF, config.equalizerQ, config.equalizerG, deviceSampleRate)),
    correctionConvolver(std::make_shared<ConvolutionReverb>(config.correctionToggle, config.correctionIRFilePath, config.correctionDryWet, deviceSampleRate, config.correctionPostGain)),
    convolutionReverb(std::make_shared<ConvolutionReverb>(config.reverbToggle, config.irFilePath, config.reverbDryWet, deviceSampleRate)),
    volume(volume) {}

Processing::Processing(const Config& config, const Processing* old, double volume, float deviceSampleRate) :
        config(config),
        deviceSampleRate(deviceSampleRate),
        amplifier(std::move(old->amplifier)),
        equalizer(std::move(old->equalizer)),
        correctionConvolver(std::move(old->correctionConvolver)),
        convolutionReverb(std::move(old->convolutionReverb)),
        volume(volume)
{
    if (old->volume != volume) {
        amplifier->setVolumeAdjustment(volume / old->volume);
    }
    if (amplifier->getToggle() != config.ampToggle) {
        amplifier->setToggle(config.ampToggle);
    }
    if (amplifier->getGain() != config.ampGain) {
        amplifier->setGain(config.ampGain);
    }

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

void Processing::process(std::vector<float>& input) {
    {
        std::lock_guard<std::mutex> lock(swapMutex);
        correctionConvolver->process(input);
    }

    std::vector<double> doubleInput(input.begin(), input.end());

    amplifier->process(doubleInput);
    equalizer->process(doubleInput);

    std::vector<float> processed(doubleInput.begin(), doubleInput.end());
    input = processed;

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

void Processing::setAmplifierToggle(bool toggle) {
    std::lock_guard<std::mutex> lock(swapMutex);
    amplifier->setToggle(toggle);
}

void Processing::setAmplifierGain(float gain) {
    std::lock_guard<std::mutex> lock(swapMutex);
    amplifier->setGain(gain);
}

void Processing::setAmplifierAuto(bool enabled) {
    std::lock_guard<std::mutex> lock(swapMutex);
    amplifier->setAutoGain(enabled);
}

float Processing::getAmplifierAutoGainValue() {
    std::lock_guard<std::mutex> lock(swapMutex);
    return amplifier->getAutoGainValue();
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
