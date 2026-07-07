//
// Created by Jeremi Campagna on 2024-07-16.
//

#ifndef EQ_CPP_AMPLIFIER_H
#define EQ_CPP_AMPLIFIER_H

#include <map>
#include <atomic>
#include "smoother.h"
#include "audioProcessor.h"
#include "../fileutils/globals.h"

class Amplifier: public AudioProcessor {
public:
    Amplifier(bool toggle, float gain);

    void process(std::vector<double>& input);

    float getGain();
    void setGain(float newGain);

    float getVolumeAdjustment();
    void setVolumeAdjustment(float newVolumeAdjustment);

    void setAutoGain(bool enabled);
    bool getAutoGain();
    float getAutoGainValue();
private:
    Smoother gain;
    Smoother volumeAdjustment;

    std::atomic<bool> m_autoGain{false};
    double m_rmsAccum = 0.0;
    int m_rmsCount = 0;
    double m_autoGainDb = 0.0;
};


#endif //EQ_CPP_AMPLIFIER_H
