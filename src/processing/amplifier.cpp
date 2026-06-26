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