//
// Created by Jeremi Campagna on 2024-07-17.
//

#include "equalizer.h"
#include "iostream"
#include <algorithm>


Equalizer::Equalizer(bool toggle, const std::vector<float> &fVector, const std::vector<float> &qVector, const std::vector<float> &gVector, float sampleRate) : AudioProcessor(toggle) {
    if (fVector.size() != qVector.size() || fVector.size() != gVector.size()) {
        throw std::invalid_argument("Equalizer vectors must be of the same size.");
    }

    filters = std::make_unique<std::vector<IIRFilter>>();
    for (size_t i = 0; i < fVector.size(); i++) {
        filters->emplace_back(IIRFilter(fVector[i], qVector[i], gVector[i], sampleRate));
    }

}

void Equalizer::process(std::vector<double>& input) {
    double currentMix = mix.currentValueNoChange();
    double mixRemaining = mix.getRemaining();

    if (currentMix <= 0 && mixRemaining <= 0) return;

    size_t numFrames = input.size() / 2;

    left.resize(numFrames);
    right.resize(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        left[i]  = input[2 * i];
        right[i] = input[2 * i + 1];
    }

    for (auto &filter: *filters) {
        filter.process(left);
    }
    for (auto &filter: *filters) {
        std::vector<double> savedLeft;
        std::swap(filter.state, savedLeft);
        std::swap(filter.state, filter.rightState);
        filter.process(right);
        std::swap(filter.state, filter.rightState);
        std::swap(filter.state, savedLeft);
    }

    if (mixRemaining <= 0) {
        double dw = currentMix;
        for (size_t i = 0; i < numFrames; ++i) {
            input[2 * i]     = left[i]  * dw + input[2 * i]     * (1.0 - dw);
            input[2 * i + 1] = right[i] * dw + input[2 * i + 1] * (1.0 - dw);
        }
    } else {
        for (size_t i = 0; i < numFrames; ++i) {
            double dw = mix.currentValue();
            input[2 * i]     = left[i]  * dw + input[2 * i]     * (1.0 - dw);
            input[2 * i + 1] = right[i] * dw + input[2 * i + 1] * (1.0 - dw);
        }
    }
}

std::vector<IIRFilter>& Equalizer::getFilters() {
    return *filters;
}