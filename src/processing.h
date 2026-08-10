//
// Created by Jeremi Campagna on 2024-07-16.
//

#ifndef EQ_CPP_PROCESSING_H
#define EQ_CPP_PROCESSING_H

#include <map>
#include <atomic>
#include "./processing/audioProcessor.h"
#include "./processing/equalizer.h"
#include "./processing/convolutionReverb.h"
#include "./processing/binauralRenderer.h"
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
    void setEqualizerBand(int index, float f, float q, float g);

    void setBinauralToggle(bool toggle);
    void setBinauralDryWet(double dryWet);
    void setBinauralAngle(int angle);
    void setBinauralConfig(const std::string& config);

    void setBinauralRoom(const std::string& room, const std::string& configOverride = "");
    void setBinauralTrueStereo(bool enabled);
    void setBinauralTargetDuration(float sec);
    void processBinaural(std::vector<float>& buffer);

    void setCorrectionPreGain(float db);
    void setEqualizerPreGain(float db);
    void setReverbPreGain(float db);
    void setBinauralPreGain(float db);

    void setMasterGain(float db);
    int consumeClipCount_Master();

    struct IRStatus {
        bool hasFile;
        bool loaded;
        float duration;
    };
    IRStatus getCorrectionIRStatus() const;
    IRStatus getReverbIRStatus() const;

    int consumeClipCount_Correction();
    int consumeClipCount_Equalizer();
    int consumeClipCount_Reverb();
    int consumeClipCount_Binaural();
private:
    double volume;
    float deviceSampleRate;
    Config config;
    std::shared_ptr<Equalizer> equalizer;
    std::shared_ptr<ConvolutionReverb> correctionConvolver;
    std::shared_ptr<ConvolutionReverb> convolutionReverb;
    std::shared_ptr<BinauralRenderer> binauralRenderer;
    std::mutex swapMutex;

    std::atomic<float> m_correctionPreGain{0.0f};
    std::atomic<float> m_equalizerPreGain{0.0f};
    std::atomic<float> m_reverbPreGain{0.0f};
    std::atomic<float> m_binauralPreGain{0.0f};
    std::atomic<float> m_masterGain{0.0f};
    std::atomic<int> m_masterClipCount{0};
};


#endif //EQ_CPP_PROCESSING_H
