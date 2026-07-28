#ifndef BINAURAL_RENDERER_H
#define BINAURAL_RENDERER_H

#include <vector>
#include <string>
#include <Accelerate/Accelerate.h>
#include <atomic>
#include <mutex>
#include "smoother.h"
#include "../fileutils/readIRFile.h"
#include "../fileutils/globals.h"

class BinauralRenderer {
public:
    BinauralRenderer(bool toggle, const std::string& brirDir, const std::string& config, int angle, float deviceSampleRate, bool trueStereo = false);
    ~BinauralRenderer();

    void processInterleaved(std::vector<float>& buffer);

    bool getToggle() const { return m_toggle.load(std::memory_order_relaxed); }
    void setToggle(bool v);

    double getDryWet() { return m_dryWet.currentValueNoChange(); }
    void setDryWet(double v);

    void setAngle(int degrees);
    int getAngle() const { return m_angle.load(std::memory_order_relaxed); }

    void setConfig(const std::string& config);

    void setDir(const std::string& dir, const std::string& configOverride = "");
    void setTrueStereo(bool enabled);
    bool isTrueStereo() const { return m_trueStereo; }

    size_t getNumChunks() const { return m_numChunks; }
    float getIRDurationSec() const { return m_irDuration; }

    void setTargetDuration(float sec);

    static std::vector<std::string> availableRooms();

    struct RoomInfo {
        std::string id;
        std::string name;
        std::string location;
        std::string type;
        std::string dimensions;
        std::string rt60;
        std::string listener;
        std::string sourceDistance;
        std::string azimuthRange;

        std::string measurementConfig;
    };
    static std::vector<RoomInfo> loadRoomInfos();

    struct SplitComplexV {
        std::vector<float> real;
        std::vector<float> imag;
        DSPSplitComplex dsp() { return { real.data(), imag.data() }; }
    };

private:
    void loadBRIRUnlocked();
    void allocateBuffers();
    void clearOverlap();
    void beginCrossfade();

    std::atomic<bool> m_toggle;
    Smoother m_dryWet;
    std::atomic<int> m_angle;
    std::string m_brirDir;
    std::string m_config;

    bool m_trueStereo;
    float m_deviceSampleRate;

    size_t m_chunkSize;
    size_t m_paddedSize;
    size_t m_numBins;
    size_t m_numChunks;
    float m_irDuration;
    float m_targetDuration;

    FFTSetup m_fftSetup;

    std::vector<SplitComplexV> m_irFFTsNegL;
    std::vector<SplitComplexV> m_irFFTsNegR;
    std::vector<SplitComplexV> m_irFFTsPosL;
    std::vector<SplitComplexV> m_irFFTsPosR;
    std::vector<SplitComplexV> m_irFFTsCrossNL;
    std::vector<SplitComplexV> m_irFFTsCrossNR;
    std::vector<SplitComplexV> m_irFFTsCrossPL;
    std::vector<SplitComplexV> m_irFFTsCrossPR;

    SplitComplexV m_fftIn;
    std::vector<float> m_inputBuf;

    // persistent overlap-save state
    size_t m_totalOvl;
    std::vector<float> m_ovlNegL;
    std::vector<float> m_ovlNegR;
    std::vector<float> m_ovlPosL;
    std::vector<float> m_ovlPosR;
    std::vector<float> m_ovlCrossNL;
    std::vector<float> m_ovlCrossNR;
    std::vector<float> m_ovlCrossPL;
    std::vector<float> m_ovlCrossPR;
    SplitComplexV m_worker1;
    SplitComplexV m_worker2;
    SplitComplexV m_worker3;
    SplitComplexV m_worker4;
    SplitComplexV m_workerCrossN;
    SplitComplexV m_workerCrossP;
    std::vector<float> m_iblitted1;
    std::vector<float> m_iblitted2;
    std::vector<float> m_iblitted3;
    std::vector<float> m_iblitted4;
    std::vector<float> m_iblittedCrossN;
    std::vector<float> m_iblittedCrossP;

    // crossfade state (anti-click on IR swap)
    size_t m_xfadeRemaining;       // frames remaining in fade-out region
    size_t m_xfadePauseRemaining;  // frames of silence during IR swap
    size_t m_xfadeInRemaining;     // frames of fade-in after swap
    size_t m_xfadeDrainFadePos;    // position within current fade phase
    bool m_xfadeActive;

    std::vector<float> m_accumL;
    std::vector<float> m_accumR;
    std::vector<float> m_drainL;
    std::vector<float> m_drainR;

    std::mutex m_mutex;

    static bool loadBRIRFile(const std::string& path, float sampleRate,
                             size_t chunkSize, size_t paddedSize, size_t numBins,
                             FFTSetup fftSetup,
                             std::vector<SplitComplexV>& outL, std::vector<SplitComplexV>& outR,
                             size_t& numChunks);
    void ifftZ(SplitComplexV& sc, std::vector<float>& out);
    void convolveChannel(const std::vector<float>& input,
                         const std::vector<SplitComplexV>& irFFTs,
                         std::vector<float>& overlap,
                         SplitComplexV& workerReal,
                         std::vector<float>& iblitted);
};

#endif
