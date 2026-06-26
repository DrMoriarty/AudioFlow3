//
// Created by Jeremi Campagna on 2024-07-16.
//

#ifndef EQ_CPP_PROCESSING_H
#define EQ_CPP_PROCESSING_H

#include <map>
#include "./processing/audioProcessor.h"
#include "./processing/amplifier.h"
#include "./processing/equalizer.h"
#include "./processing/convolutionReverb.h"
#include "./fileutils/config.h"
#include "./fileutils/readIRFile.h"
#include "./processing/smoother.h"

class Processing {
public:
    Processing(const Config& config, double volume, float deviceSampleRate);
    Processing(const Config& config, const Processing* old, double volume, float deviceSampleRate);

    void process(std::vector<float>& input);

    void setReverbToggle(bool toggle);
    void setReverbDryWet(double dryWet);
    void setReverbIRFile(const std::string& path);

    void setCorrectionToggle(bool toggle);
    void setCorrectionDryWet(double dryWet);
    void setCorrectionIRFile(const std::string& path);
    void setCorrectionPostGain(float postGainDb);

    void setEqualizerToggle(bool toggle);
    void setAmplifierToggle(bool toggle);
    void setAmplifierGain(float gain);
    void setEqualizerBand(int index, float f, float q, float g);
private:
    double volume;
    float deviceSampleRate;
    Config config;
    std::shared_ptr<Amplifier> amplifier;
    std::shared_ptr<Equalizer> equalizer;
    std::shared_ptr<ConvolutionReverb> correctionConvolver;
    std::shared_ptr<ConvolutionReverb> convolutionReverb;
    std::mutex swapMutex;
};


#endif //EQ_CPP_PROCESSING_H
