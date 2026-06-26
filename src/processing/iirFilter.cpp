//
// Created by Jeremi Campagna on 2024-07-17.
//

#include "iirFilter.h"

IIRFilter::IIRFilter(float f, float q, float g, float sampleRate)
    : frequency(Smoother(f, f, 0)), quality(Smoother(q, q, 0)), gain(Smoother(g, g, 0)), sampleRate(Smoother(sampleRate, sampleRate, 0)) {

    calculatePeakFilter();
    state = std::vector<double>(a_coeffs.size(), 0.0);
    rightState = std::vector<double>(a_coeffs.size(), 0.0);
}

void IIRFilter::process(std::vector<double>& input) {
    bool transitioning = frequency.getRemaining() > 0 || quality.getRemaining() > 0 || gain.getRemaining() > 0;

    if (!transitioning) {
        const double b0 = b_coeffs[0];
        const double b1 = b_coeffs[1];
        const double b2 = b_coeffs[2];
        const double a1 = a_coeffs[1];
        const double a2 = a_coeffs[2];
            
        for (size_t n = 0; n < input.size(); ++n) {
            double w0 = input[n] - a1 * state[0] - a2 * state[1];
            double yn = b0 * w0 + b1 * state[0] + b2 * state[1];
            input[n] = yn;

            for (size_t i = state.size() - 1; i > 0; --i) {
                state[i] = state[i - 1];
            }
            if (!state.empty()) {
                state[0] = w0;
            }
        }
    } else {
        for (size_t n = 0; n < input.size(); ++n) {

            calculatePeakFilter();

            double w0 = input[n] - a_coeffs[1] * state[0] - a_coeffs[2] * state[1];
            double yn = b_coeffs[0] * w0 + b_coeffs[1] * state[0] + b_coeffs[2] * state[1];
            input[n] = yn;

            for (size_t i = state.size() - 1; i > 0; --i) {
                state[i] = state[i - 1];
            }
            if (!state.empty()) {
                state[0] = w0;
            }
        }
    }
}

static const double LN10_OVER_40 = std::log(10.0) / 40.0;

void IIRFilter::calculatePeakFilter() {
    double A = std::exp(LN10_OVER_40 * gain.currentValue());
    double omega = 2.0 * M_PI * frequency.currentValue() / sampleRate.currentValue();
    double sinOmega = std::sin(omega);
    double cosOmega = std::cos(omega);
    double alpha = sinOmega / (2.0 * quality.currentValue());

    double a0_inv = 1.0 / (1.0 + alpha / A);
    double a1 = -2.0 * cosOmega;
    double a2 = 1.0 - alpha / A;
    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cosOmega;
    double b2 = 1.0 - alpha * A;

    a_coeffs = {1.0, a1 * a0_inv, a2 * a0_inv};
    b_coeffs = {b0 * a0_inv, b1 * a0_inv, b2 * a0_inv};

}

float IIRFilter::getFrequency() {
    return frequency.currentValueNoChange();
}

void IIRFilter::setFrequency(float newFrequency) {
    frequency = Smoother(frequency.currentValueNoChange(), newFrequency, smootherSteps);
}

float IIRFilter::getQuality() {
    return quality.currentValueNoChange();
}

void IIRFilter::setQuality(float newQuality) {
    quality = Smoother(quality.currentValueNoChange(), newQuality, smootherSteps);
}

float IIRFilter::getGain() {
    return gain.currentValueNoChange();
}

void IIRFilter::setGain(float newGain) {
    gain = Smoother(gain.currentValueNoChange(), newGain, smootherSteps);
}
