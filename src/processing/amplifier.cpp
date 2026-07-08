//
// Created by Jeremi Campagna on 2024-07-16.
//

#include "amplifier.h"
#include <cmath>
#include <algorithm>

static const double LN10_OVER_20 = std::log(10.0) / 20.0;

Amplifier::Amplifier(bool toggle, float gain) : AudioProcessor(toggle), gain(Smoother(gain, gain, 0)), volumeAdjustment(Smoother(1.0, 1.0, smootherSteps)) {}

void Amplifier::process(std::vector<double> &input) {
    double currentMix = mix.currentValueNoChange();
    double mixRemaining = mix.getRemaining();

    if (currentMix <= 0 && mixRemaining <= 0) return;

    if (m_autoGain.load(std::memory_order_relaxed)) {
        constexpr double TARGET_DB = -14.0;
        constexpr double SMOOTH = 0.005;

        for (auto &sample : input) {
            m_rmsAccum += sample * sample;
            m_rmsCount++;
        }

        if (m_rmsCount > 0) {
            double rms = std::sqrt(m_rmsAccum / m_rmsCount);
            m_rmsCount = 0;
            m_rmsAccum = 0.0;
            if (rms > 1e-10) {
                double inputDb = 20.0 * std::log10(rms);
                double correction = TARGET_DB - inputDb;
                m_autoGainDb += (correction - m_autoGainDb) * SMOOTH;
            }
        }

        gain = Smoother(m_autoGainDb, m_autoGainDb, 0);
    }

    bool gainConst = gain.getRemaining() <= 0;
    bool volConst = volumeAdjustment.getRemaining() <= 0;
    bool mixConst = mixRemaining <= 0;

    if (gainConst && volConst && mixConst) {
        double gainLin = std::exp(LN10_OVER_20 * gain.currentValueNoChange());
        double va = volumeAdjustment.currentValueNoChange();
        double factor = gainLin * va * currentMix + (1.0 - currentMix);
        std::transform(input.begin(), input.end(), input.begin(),
            [factor](double s) { return s * factor; });
        return;
    }

    double gainVal = gain.currentValueNoChange();
    double volVal = volumeAdjustment.currentValueNoChange();

    if (gainConst && volConst) {
        double scaledBase = std::exp(LN10_OVER_20 * gainVal) * volVal;
        for (auto &sample : input) {
            double m = mix.currentValue();
            sample = sample * scaledBase * m + sample * (1.0 - m);
        }
        return;
    }

    for (auto &sample : input) {
        double g = gainConst ? gainVal : gain.currentValue();
        double m = mixConst ? currentMix : mix.currentValue();
        double v = volConst ? volVal : volumeAdjustment.currentValue();
        double scaled = sample * std::exp(LN10_OVER_20 * g) * v;
        sample = scaled * m + sample * (1.0 - m);
    }
}

float Amplifier::getGain() {
    return gain.currentValueNoChange();
}

void Amplifier::setGain(float newGain) {
    gain = Smoother(gain.currentValueNoChange(), newGain, smootherSteps);
}

float Amplifier::getVolumeAdjustment() {
    return volumeAdjustment.currentValueNoChange();
}

void Amplifier::setVolumeAdjustment(float newVolumeAdjustment) {
    volumeAdjustment = Smoother(newVolumeAdjustment, 1.0, volumeSmootherSteps);
}

void Amplifier::setAutoGain(bool enabled) {
    m_autoGain.store(enabled, std::memory_order_relaxed);
}

bool Amplifier::getAutoGain() {
    return m_autoGain.load(std::memory_order_relaxed);
}

float Amplifier::getAutoGainValue() {
    return static_cast<float>(m_autoGainDb);
}
