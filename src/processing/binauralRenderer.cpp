#include "binauralRenderer.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <thread>
#include <sstream>
#include <iomanip>
#include <dirent.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>




static std::string getBrirBasePath() {
    static std::string cached;
    if (!cached.empty()) return cached;
    CFURLRef url = CFBundleCopyExecutableURL(CFBundleGetMainBundle());
    if (url) {
        CFURLRef dir = CFURLCreateCopyDeletingLastPathComponent(nullptr, url);
        char buf[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(dir, true, reinterpret_cast<UInt8*>(buf), PATH_MAX))
            cached = std::string(buf) + "/../Resources/brir";
        if (dir) CFRelease(dir);
        CFRelease(url);
    }
    return cached;
}

static std::vector<int> scanAngles(const std::string& roomDir, const std::string& config) {
    std::vector<int> angles;
    std::string prefix = "_" + config + "_E0_A";
    DIR* d = opendir(roomDir.c_str());
    if (!d) return angles;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name(ent->d_name);
        auto pos = name.find(prefix);
        if (pos == std::string::npos) continue;
        if (name.find("_True_Stereo") != std::string::npos) continue;
        size_t start = pos + prefix.size();
        size_t end = name.find('.', start);
        if (end == std::string::npos) continue;
        std::string numStr = name.substr(start, end - start);
        try {
            angles.push_back(std::stoi(numStr));
        } catch (...) {}
    }
    closedir(d);
    std::sort(angles.begin(), angles.end());
    return angles;
}

static int nearestAngle(const std::vector<int>& angles, int target) {
    if (angles.empty()) return target;
    int best = angles[0];
    int bestDist = std::abs(target - best);
    for (int a : angles) {
        int d = std::abs(target - a);
        if (d < bestDist) { bestDist = d; best = a; }
    }
    return best;
}

static std::string angleName(int angle) {
    std::ostringstream oss;
    if (angle < 0) oss << "A" << angle;
    else oss << "A" << angle;
    return oss.str();
}

static std::string brirFilePath(const std::string& room, const std::string& roomDir, const std::string& config, int angle) {
    return roomDir + "/BRIR_" + room + "_" + config + "_E0_" + angleName(angle) + ".wav";
}

void BinauralRenderer::allocateBuffers() {
    m_totalOvl = (m_numChunks + 2) * m_chunkSize;
    m_fftIn = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_inputBuf.resize(m_paddedSize, 0.0f);
    m_ovlNegL.resize(m_totalOvl, 0.0f);
    m_ovlNegR.resize(m_totalOvl, 0.0f);
    m_ovlPosL.resize(m_totalOvl, 0.0f);
    m_ovlPosR.resize(m_totalOvl, 0.0f);
    m_ovlCrossNL.resize(m_totalOvl, 0.0f);
    m_ovlCrossNR.resize(m_totalOvl, 0.0f);
    m_ovlCrossPL.resize(m_totalOvl, 0.0f);
    m_ovlCrossPR.resize(m_totalOvl, 0.0f);
    m_worker1 = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_worker2 = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_worker3 = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_worker4 = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_workerCrossN = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_workerCrossP = SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)};
    m_iblitted1.resize(m_paddedSize, 0.0f);
    m_iblitted2.resize(m_paddedSize, 0.0f);
    m_iblitted3.resize(m_paddedSize, 0.0f);
    m_iblitted4.resize(m_paddedSize, 0.0f);
    m_iblittedCrossN.resize(m_paddedSize, 0.0f);
    m_iblittedCrossP.resize(m_paddedSize, 0.0f);
    clearOverlap();
    m_drainL.clear();
    m_drainR.clear();
    m_accumL.clear();
    m_accumR.clear();
}

void BinauralRenderer::clearOverlap() {
    std::fill(m_ovlNegL.begin(), m_ovlNegL.end(), 0.0f);
    std::fill(m_ovlNegR.begin(), m_ovlNegR.end(), 0.0f);
    std::fill(m_ovlPosL.begin(), m_ovlPosL.end(), 0.0f);
    std::fill(m_ovlPosR.begin(), m_ovlPosR.end(), 0.0f);
    std::fill(m_ovlCrossNL.begin(), m_ovlCrossNL.end(), 0.0f);
    std::fill(m_ovlCrossNR.begin(), m_ovlCrossNR.end(), 0.0f);
    std::fill(m_ovlCrossPL.begin(), m_ovlCrossPL.end(), 0.0f);
    std::fill(m_ovlCrossPR.begin(), m_ovlCrossPR.end(), 0.0f);
}

void BinauralRenderer::beginCrossfade() {
    const size_t phaseLen = m_chunkSize * 2;  // ~46ms per phase at 44.1kHz
    m_xfadeRemaining      = phaseLen;
    m_xfadePauseRemaining = phaseLen;
    m_xfadeInRemaining    = phaseLen;
    m_xfadeDrainFadePos   = 0;
    m_xfadeActive         = true;
}

BinauralRenderer::BinauralRenderer(bool toggle, const std::string& brirDir, const std::string& config, int angle, float deviceSampleRate, bool trueStereo)
    : m_toggle(toggle)
    , m_dryWet(Smoother(1.0, 1.0, 0))
    , m_angle(angle)
    , m_brirDir(brirDir)
    , m_config(config)
    , m_trueStereo(trueStereo)
    , m_deviceSampleRate(deviceSampleRate)
    , m_chunkSize(static_cast<size_t>(convolutionChunkSize))
    , m_paddedSize(m_chunkSize * 2)
    , m_numBins(m_paddedSize / 2)
    , m_numChunks(0)
    , m_irDuration(0.0f)
    , m_xfadeRemaining(0)
    , m_xfadePauseRemaining(0)
    , m_xfadeInRemaining(0)
    , m_xfadeDrainFadePos(0)
    , m_xfadeActive(false)
{
    m_fftSetup = vDSP_create_fftsetup(static_cast<vDSP_Length>(std::log2(m_paddedSize)), FFT_RADIX2);
    loadBRIRUnlocked();
}

BinauralRenderer::~BinauralRenderer() {
    if (m_fftSetup)
        vDSP_destroy_fftsetup(m_fftSetup);
}

bool BinauralRenderer::loadBRIRFile(const std::string& path, float sampleRate,
                                     size_t chunkSize, size_t paddedSize, size_t numBins,
                                     FFTSetup fftSetup,
                                     std::vector<SplitComplexV>& outL, std::vector<SplitComplexV>& outR,
                                     size_t& numChunks) {
    IRData irData = readIRFile(path, static_cast<uint32_t>(sampleRate));
    if (irData.audioDataL.empty() || irData.audioDataR.empty()) {
        numChunks = 0;
        return false;
    }

    numChunks = static_cast<size_t>(std::ceil(static_cast<float>(irData.audioDataL.size()) / chunkSize));

    irData.audioDataL.resize(numChunks * chunkSize, 0.0f);
    irData.audioDataR.resize(numChunks * chunkSize, 0.0f);

    SplitComplexV fftTmp{std::vector<float>(numBins), std::vector<float>(numBins)};
    std::vector<float> chunkBuf(paddedSize, 0.0f);

    outL.clear();
    outR.clear();

    auto fftChannel = [&](const std::vector<float>& data, std::vector<SplitComplexV>& dst) {
        for (size_t i = 0; i < numChunks; ++i) {
            std::copy(data.begin() + i * chunkSize, data.begin() + (i + 1) * chunkSize, chunkBuf.begin());
            std::fill(chunkBuf.begin() + chunkSize, chunkBuf.end(), 0.0f);

            DSPSplitComplex sc = { fftTmp.real.data(), fftTmp.imag.data() };
            vDSP_ctoz(reinterpret_cast<const DSPComplex*>(chunkBuf.data()), 2, &sc, 1, numBins);
            vDSP_fft_zrip(fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(paddedSize)), FFT_FORWARD);
            float scale = 0.5f;
            vDSP_vsmul(fftTmp.real.data(), 1, &scale, fftTmp.real.data(), 1, numBins);
            vDSP_vsmul(fftTmp.imag.data(), 1, &scale, fftTmp.imag.data(), 1, numBins);

            SplitComplexV d{std::vector<float>(numBins), std::vector<float>(numBins)};
            std::copy(fftTmp.real.begin(), fftTmp.real.end(), d.real.begin());
            std::copy(fftTmp.imag.begin(), fftTmp.imag.end(), d.imag.begin());
            dst.push_back(std::move(d));
        }
    };

    fftChannel(irData.audioDataL, outL);
    fftChannel(irData.audioDataR, outR);
    return true;
}

static void rawAudioToFft(std::vector<float>& inAudio,
                           size_t chunkSize, size_t paddedSize, size_t numBins,
                           FFTSetup fftSetup,
                           std::vector<float>& chunkBuf,
                           BinauralRenderer::SplitComplexV& fftTmp,
                           std::vector<BinauralRenderer::SplitComplexV>& outSplit) {
    size_t numChunks = inAudio.size() / chunkSize + (inAudio.size() % chunkSize ? 1 : 0);
    for (size_t i = 0; i < numChunks; ++i) {
        auto itS = inAudio.begin() + i * chunkSize;
        auto itE = itS + chunkSize;
        if (itE > inAudio.end()) itE = inAudio.end();
        std::copy(itS, itE, chunkBuf.begin());
        std::fill(chunkBuf.begin() + chunkSize, chunkBuf.end(), 0.0f);
        DSPSplitComplex sc = { fftTmp.real.data(), fftTmp.imag.data() };
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(chunkBuf.data()), 2, &sc, 1, numBins);
        vDSP_fft_zrip(fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(paddedSize)), FFT_FORWARD);
        float scale = 0.5f;
        vDSP_vsmul(fftTmp.real.data(), 1, &scale, fftTmp.real.data(), 1, numBins);
        vDSP_vsmul(fftTmp.imag.data(), 1, &scale, fftTmp.imag.data(), 1, numBins);
        BinauralRenderer::SplitComplexV scv{std::vector<float>(numBins), std::vector<float>(numBins)};
        std::copy(fftTmp.real.begin(), fftTmp.real.end(), scv.real.begin());
        std::copy(fftTmp.imag.begin(), fftTmp.imag.end(), scv.imag.begin());
        outSplit.push_back(std::move(scv));
    }
}

void BinauralRenderer::loadBRIRUnlocked() {
    std::string roomDir = getBrirBasePath() + "/" + m_brirDir;

    if (m_trueStereo) {
        std::string tsPath = roomDir + "/BRIR_" + m_brirDir + "_" + m_config + "_True_Stereo.wav";
        auto tsData = read4chWavFile(tsPath, static_cast<uint32_t>(m_deviceSampleRate));
        if (!tsData.ok) {
            std::cerr << "loadBRIRUnlocked: trueStereo file not found: " << tsPath << std::endl;
            m_numChunks = 0;
            m_irDuration = 0.0f;
            allocateBuffers();
            return;
        }
        m_numChunks = tsData.ch0.size() / m_chunkSize + (tsData.ch0.size() % m_chunkSize ? 1 : 0);
        std::vector<SplitComplexV> fftLL, fftLR, fftRL, fftRR;
        std::vector<float> chunkBuf(m_paddedSize, 0.0f);
        SplitComplexV fftTmp;
        fftTmp.real.resize(m_numBins, 0.0f);
        fftTmp.imag.resize(m_numBins, 0.0f);
        rawAudioToFft(tsData.ch0, m_chunkSize, m_paddedSize, m_numBins, m_fftSetup, chunkBuf, fftTmp, fftLL);
        rawAudioToFft(tsData.ch1, m_chunkSize, m_paddedSize, m_numBins, m_fftSetup, chunkBuf, fftTmp, fftLR);
        rawAudioToFft(tsData.ch2, m_chunkSize, m_paddedSize, m_numBins, m_fftSetup, chunkBuf, fftTmp, fftRL);
        rawAudioToFft(tsData.ch3, m_chunkSize, m_paddedSize, m_numBins, m_fftSetup, chunkBuf, fftTmp, fftRR);

        size_t nc = m_numChunks;
        m_irFFTsNegL.clear();
        m_irFFTsNegR.clear();
        m_irFFTsPosL.clear();
        m_irFFTsPosR.clear();
        m_irFFTsCrossNL.clear();
        m_irFFTsCrossNR.clear();
        m_irFFTsCrossPL.clear();
        m_irFFTsCrossPR.clear();
        for (size_t i = 0; i < nc; ++i) {
            m_irFFTsNegL.push_back(fftLL[i]);
            m_irFFTsNegR.push_back(fftLR[i]);
            m_irFFTsCrossNL.push_back(fftLR[i]);
            m_irFFTsCrossNR.push_back(fftRR[i]);
        }
        for (size_t i = 0; i < nc; ++i) {
            m_irFFTsPosL.push_back(fftLL[i]);
            m_irFFTsPosR.push_back(fftRR[i]);
            m_irFFTsCrossPL.push_back(fftRL[i]);
            m_irFFTsCrossPR.push_back(fftRR[i]);
        }
        m_irDuration = static_cast<float>(tsData.ch0.size()) / m_deviceSampleRate;
        m_targetDuration = m_irDuration;
        allocateBuffers();
        return;
    }

    auto angles = scanAngles(roomDir, m_config);
    int angle = nearestAngle(angles, m_angle.load(std::memory_order_relaxed));
    m_angle.store(angle, std::memory_order_relaxed);

    std::vector<SplitComplexV> negL, negR, posL, posR;
    size_t numChunksNeg = 0, numChunksPos = 0;

    std::string pathPos = brirFilePath(m_brirDir, roomDir, m_config, angle);
    loadBRIRFile(pathPos, m_deviceSampleRate, m_chunkSize, m_paddedSize, m_numBins,
                 m_fftSetup, posL, posR, numChunksPos);

    if (angle == 0 || angle == 180 || angle == -180) {
        negL = posL;
        negR = posR;
        numChunksNeg = numChunksPos;
    } else {
        std::string pathNeg = brirFilePath(m_brirDir, roomDir, m_config, -angle);
        loadBRIRFile(pathNeg, m_deviceSampleRate, m_chunkSize, m_paddedSize, m_numBins,
                     m_fftSetup, negL, negR, numChunksNeg);
    }

    m_numChunks = std::max(numChunksNeg, numChunksPos);
    if (m_targetDuration > 0.0f) {
        size_t cap = static_cast<size_t>(std::ceil(m_targetDuration * m_deviceSampleRate / m_chunkSize));
        m_numChunks = std::min(m_numChunks, cap);

        auto trimChunks = [&](std::vector<SplitComplexV>& v) {
            if (v.size() > m_numChunks) v.resize(m_numChunks);
        };
        trimChunks(negL); trimChunks(negR);
        trimChunks(posL); trimChunks(posR);
    }
    if (m_numChunks == 0) {
        m_irDuration = 0.0f;
        m_irFFTsNegL.clear(); m_irFFTsNegR.clear();
        m_irFFTsPosL.clear(); m_irFFTsPosR.clear();
        allocateBuffers();
        return;
    }

    auto ensureChunkCount = [&](std::vector<SplitComplexV>& v) {
        while (v.size() < m_numChunks) {
            v.push_back(SplitComplexV{std::vector<float>(m_numBins), std::vector<float>(m_numBins)});
        }
    };
    ensureChunkCount(negL); ensureChunkCount(negR);
    ensureChunkCount(posL); ensureChunkCount(posR);

    m_irFFTsNegL = std::move(negL);
    m_irFFTsNegR = std::move(negR);
    m_irFFTsPosL = std::move(posL);
    m_irFFTsPosR = std::move(posR);

    std::string anyPath = (numChunksPos > 0) ? pathPos : brirFilePath(m_brirDir, roomDir, m_config, -angle);
    IRData probe = readIRFile(anyPath, static_cast<uint32_t>(m_deviceSampleRate));
    m_irDuration = probe.audioDataL.empty() ? 0.0f : static_cast<float>(probe.audioDataL.size()) / m_deviceSampleRate;

    allocateBuffers();
}

std::vector<std::string> BinauralRenderer::availableRooms() {
    std::vector<std::string> rooms;
    std::string brirDir = getBrirBasePath();
    DIR* d = opendir(brirDir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_type != DT_DIR) continue;
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            rooms.push_back(name);
        }
        closedir(d);
    }
    std::sort(rooms.begin(), rooms.end());
    return rooms;
}

void BinauralRenderer::ifftZ(SplitComplexV& sc, std::vector<float>& out) {
    DSPSplitComplex z = sc.dsp();
    vDSP_fft_zrip(m_fftSetup, &z, 1, static_cast<vDSP_Length>(std::log2(m_paddedSize)), FFT_INVERSE);
    vDSP_ztoc(&z, 1, reinterpret_cast<COMPLEX*>(out.data()), 2, m_numBins);
    float factor = 1.0f / static_cast<float>(m_paddedSize);
    vDSP_vsmul(out.data(), 1, &factor, out.data(), 1, m_paddedSize);
}

void BinauralRenderer::convolveChannel(const std::vector<float>& input,
                                        const std::vector<SplitComplexV>& irFFTs,
                                        std::vector<float>& overlap,
                                        SplitComplexV& workerReal,
                                        std::vector<float>& iblitted) {
    std::fill(m_inputBuf.begin(), m_inputBuf.end(), 0.0f);
    std::copy(input.begin(), input.end(), m_inputBuf.begin());

    DSPSplitComplex sc = { m_fftIn.real.data(), m_fftIn.imag.data() };
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(m_inputBuf.data()), 2, &sc, 1, m_numBins);
    vDSP_fft_zrip(m_fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(m_paddedSize)), FFT_FORWARD);
    float scale = 0.5f;
    vDSP_vsmul(m_fftIn.real.data(), 1, &scale, m_fftIn.real.data(), 1, m_numBins);
    vDSP_vsmul(m_fftIn.imag.data(), 1, &scale, m_fftIn.imag.data(), 1, m_numBins);

    for (size_t i = 0; i < irFFTs.size(); ++i) {
        DSPSplitComplex inSc  = m_fftIn.dsp();
        DSPSplitComplex irSc  = const_cast<SplitComplexV&>(irFFTs[i]).dsp();
        DSPSplitComplex outSc = workerReal.dsp();
        vDSP_zvmul(&inSc, 1, &irSc, 1, &outSc, 1, m_numBins, 1);
        ifftZ(workerReal, iblitted);
        size_t base = i * m_chunkSize;
        for (size_t k = 0; k < m_paddedSize && (base + k) < overlap.size(); ++k)
            overlap[base + k] += iblitted[k];
    }
}

void BinauralRenderer::setToggle(bool v) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_toggle.store(v, std::memory_order_relaxed);
    if (v) clearOverlap();
}

void BinauralRenderer::setDryWet(double v) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dryWet = Smoother(m_dryWet.currentValueNoChange(), v, smootherSteps);
}

void BinauralRenderer::setAngle(int degrees) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string roomDir = getBrirBasePath() + "/" + m_brirDir;
    auto angles = scanAngles(roomDir, m_config);
    int snapped = nearestAngle(angles, degrees);
    if (snapped == m_angle.load(std::memory_order_relaxed)) return;
    beginCrossfade();
    m_angle.store(snapped, std::memory_order_relaxed);
    loadBRIRUnlocked();
}

void BinauralRenderer::setConfig(const std::string& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (config == m_config) return;
    beginCrossfade();
    m_config = config;
    loadBRIRUnlocked();
}

void BinauralRenderer::setDir(const std::string& dir, const std::string& configOverride) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (dir == m_brirDir && (configOverride.empty() || configOverride == m_config)) return;
    beginCrossfade();
    if (!configOverride.empty()) m_config = configOverride;
    m_brirDir = dir;
    loadBRIRUnlocked();
}


void BinauralRenderer::setTrueStereo(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (enabled == m_trueStereo) return;
    beginCrossfade();
    m_trueStereo = enabled;
    loadBRIRUnlocked();
}

void BinauralRenderer::setTargetDuration(float sec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetDuration = sec;
    loadBRIRUnlocked();
}

void BinauralRenderer::processInterleaved(std::vector<float>& buffer) {
    if (!m_toggle.load(std::memory_order_relaxed)) return;
    if (m_numChunks == 0) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    const size_t numFrames = buffer.size() / 2;

    for (size_t i = 0; i < numFrames; ++i) {
        m_accumL.push_back(buffer[2 * i]);
        m_accumR.push_back(buffer[2 * i + 1]);
    }

    std::vector<float> inChunkL(m_chunkSize, 0.0f);
    std::vector<float> inChunkR(m_chunkSize, 0.0f);
    std::vector<float> outL(m_chunkSize, 0.0f);
    std::vector<float> outR(m_chunkSize, 0.0f);

    while (m_accumL.size() >= m_chunkSize) {
        std::copy(m_accumL.begin(), m_accumL.begin() + m_chunkSize, inChunkL.begin());
        std::copy(m_accumR.begin(), m_accumR.begin() + m_chunkSize, inChunkR.begin());
        m_accumL.erase(m_accumL.begin(), m_accumL.begin() + m_chunkSize);
        m_accumR.erase(m_accumR.begin(), m_accumR.begin() + m_chunkSize);

        if (m_trueStereo) {
            convolveChannel(inChunkL, m_irFFTsNegL, m_ovlNegL, m_worker1, m_iblitted1);
            convolveChannel(inChunkL, m_irFFTsCrossNL, m_ovlCrossNL, m_workerCrossN, m_iblittedCrossN);
            convolveChannel(inChunkR, m_irFFTsPosR, m_ovlPosR, m_worker4, m_iblitted4);
            convolveChannel(inChunkR, m_irFFTsCrossPL, m_ovlCrossPL, m_workerCrossP, m_iblittedCrossP);
            for (size_t k = 0; k < m_chunkSize; ++k) {
                outL[k] = m_ovlNegL[k] + m_ovlCrossPL[k];
                outR[k] = m_ovlCrossNL[k] + m_ovlPosR[k];
            }
        } else {
            convolveChannel(inChunkL, m_irFFTsNegL, m_ovlNegL, m_worker1, m_iblitted1);
            convolveChannel(inChunkL, m_irFFTsNegR, m_ovlNegR, m_worker2, m_iblitted2);
            convolveChannel(inChunkR, m_irFFTsPosL, m_ovlPosL, m_worker3, m_iblitted3);
            convolveChannel(inChunkR, m_irFFTsPosR, m_ovlPosR, m_worker4, m_iblitted4);
            for (size_t k = 0; k < m_chunkSize; ++k) {
                outL[k] = m_ovlNegL[k] + m_ovlPosL[k];
                outR[k] = m_ovlNegR[k] + m_ovlPosR[k];
            }
        }

        m_drainL.insert(m_drainL.end(), outL.begin(), outL.end());
        m_drainR.insert(m_drainR.end(), outR.begin(), outR.end());

        auto shiftOverlap = [&](std::vector<float>& ovl) {
            std::copy(ovl.begin() + m_chunkSize, ovl.begin() + m_totalOvl, ovl.begin());
            std::fill(ovl.begin() + (m_totalOvl - m_chunkSize), ovl.end(), 0.0f);
        };
        shiftOverlap(m_ovlNegL);
        shiftOverlap(m_ovlNegR);
        shiftOverlap(m_ovlPosL);
        shiftOverlap(m_ovlPosR);
        shiftOverlap(m_ovlCrossNL);
        shiftOverlap(m_ovlCrossNR);
        shiftOverlap(m_ovlCrossPL);
        shiftOverlap(m_ovlCrossPR);
    }

    // compute amplitude envelopes for this callback
    float drainAmp = 1.0f;
    float bufAmp   = 1.0f;

    if (m_xfadeActive) {
        if (m_xfadeRemaining > 0) {
            // fading out: drain fades 1→0, buffer also fades 1→0
            const size_t phaseLen = m_chunkSize * 2;
            const float pos = static_cast<float>(m_xfadeDrainFadePos);
            const float len = static_cast<float>(phaseLen);
            m_xfadeDrainFadePos += numFrames;
            if (m_xfadeDrainFadePos >= phaseLen) {
                m_xfadeRemaining = 0;
                drainAmp = 0.0f;
                if (m_drainL.empty() || true) {
                    m_xfadePauseRemaining = phaseLen;
                    m_xfadeDrainFadePos   = 0;
                } else {
                    // drain still non-empty — continue drain at 0, wait for drain end
                    m_xfadePauseRemaining = phaseLen;
                    m_xfadeDrainFadePos   = m_drainL.size(); // start draining from where we left off
                }
            } else {
                drainAmp = 1.0f - pos / len;
                bufAmp   = drainAmp;
            }
        } else if (m_xfadePauseRemaining > 0) {
            // gap: silence. drainAmp=0 (will consume/nodrain), bufAmp=0
            drainAmp = 0.0f;
            bufAmp   = 0.0f;
            // count how many gap samples have passed using accumL as proxy
            // (we track via remaining gap samples consumed)
            if (m_xfadePauseRemaining > numFrames)
                m_xfadePauseRemaining -= numFrames;
            else
                m_xfadePauseRemaining = 0;
            if (m_xfadePauseRemaining == 0) {
                m_xfadeInRemaining  = m_chunkSize * 2;
                m_xfadeDrainFadePos = 0;
            }
        } else if (m_xfadeInRemaining > 0) {
            // fading in: drain fades 0→1, buffer also fades 0→1
            const size_t phaseLen = m_chunkSize * 2;
            const float pos = static_cast<float>(m_xfadeDrainFadePos);
            const float len = static_cast<float>(phaseLen);
            drainAmp = pos / len;
            bufAmp   = drainAmp;
            m_xfadeDrainFadePos += numFrames;
            if (m_xfadeDrainFadePos >= phaseLen) {
                m_xfadeInRemaining = 0;
                m_xfadeDrainFadePos = 0;
                m_xfadeActive = false;
            }
        }
    }

    // apply buffer amplitude (fade-out / gap / fade-in on buffer)
    if (bufAmp < 1.0f - 1e-6f) {
        for (size_t i = 0; i < numFrames; ++i) {
            buffer[2 * i]     *= bufAmp;
            buffer[2 * i + 1] *= bufAmp;
        }
    }

    bool mixConst = m_dryWet.getRemaining() <= 0;

    // drain: consume existing drain data, apply drainAmp envelope
    while (numFrames > 0 && m_drainL.size() >= numFrames) {
        for (size_t i = 0; i < numFrames; ++i) {
            float wetL = m_drainL[i] * drainAmp;
            float wetR = m_drainR[i] * drainAmp;
            float scale = mixConst ? static_cast<float>(m_dryWet.currentValueNoChange())
                                   : static_cast<float>(m_dryWet.currentValue());
            float oneMinus = 1.0f - scale;
            buffer[2 * i]     = wetL * scale + buffer[2 * i]     * oneMinus;
            buffer[2 * i + 1] = wetR * scale + buffer[2 * i + 1] * oneMinus;
        }
        m_drainL.erase(m_drainL.begin(), m_drainL.begin() + numFrames);
        m_drainR.erase(m_drainR.begin(), m_drainR.begin() + numFrames);
        break;
    }
}

static std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

std::vector<BinauralRenderer::RoomInfo> BinauralRenderer::loadRoomInfos() {
    std::vector<RoomInfo> rooms;
    std::string csvPath = getBrirBasePath() + "/Rooms.csv";
    std::ifstream file(csvPath);
    if (!file.is_open()) return rooms;

    std::string headerLine;
    std::getline(file, headerLine);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto f = parseCsvLine(line);
        if (f.size() < 12) continue;
        RoomInfo r;
        r.id             = f[0];
        r.name           = f[1];
        r.location       = f[2];
        r.type           = f[3];
        r.dimensions     = f[4];
        r.rt60           = f[6];
        r.listener       = f[7];
        r.sourceDistance  = f[8];
        r.azimuthRange   = f[9];

        r.measurementConfig = f[11];
        rooms.push_back(std::move(r));
    }
    return rooms;
}
