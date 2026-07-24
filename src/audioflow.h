#ifndef AUDIO_FLOW_H
#define AUDIO_FLOW_H

#include "./fileutils/config.h"

bool initialize(const std::string& configPath);
void cleanup();

const Config& getConfig();

std::string getCurrentOutputDeviceName();
std::vector<std::string> getAvailableOutputDevices();
bool setOutputDevice(const std::string& name);
void setReverbToggle(bool toggle);
void setReverbDryWet(double dryWet);
void setReverbIRFile(const std::string& path);
void setCorrectionToggle(bool toggle);
void setCorrectionIRFile(const std::string& path);
void setCorrectionDryWet(double dryWet);
void setCorrectionPostGain(float postGain);
void setEqualizerToggle(bool toggle);
void setAmplifierToggle(bool toggle);
void setAmplifierGain(float gain);
void setAmplifierAuto(bool enabled);
float getAmplifierAutoGainValue();
void setEqualizerBand(int index, float f, float q, float g);
void setEqualizerPreset(const std::string& name);
void setBufferSize(int newBufSize);
float getLatencyMs();
float getProcessTimeMs();
float getPeakLevelL();
float getPeakLevelR();

struct IRStatusInfo {
    bool hasFile;
    bool loaded;
    float duration;
};
IRStatusInfo getCorrectionIRStatus();
IRStatusInfo getReverbIRStatus();

void setUIExpandedCorrecting(bool expanded);
void setUIExpandedPreamplifier(bool expanded);
void setUIExpandedEqualizer(bool expanded);
void setUIExpandedReverb(bool expanded);
void setUIExpandedSettings(bool expanded);
void setUIExpandedBinaural(bool expanded);

void setBinauralToggle(bool toggle);
void setBinauralDryWet(double dryWet);
void setBinauralAngle(int angle);
void setBinauralConfig(const std::string& config);
void setBinauralElevation(int elevation);
void setBinauralTargetDuration(float sec);
void setBinauralRoom(const std::string& room, const std::string& configOverride = "");
void setBinauralTrueStereo(bool enabled);
std::vector<std::string> getBinauralRooms();

struct RoomInfoData {
    std::string id;
    std::string name;
    std::string location;
    std::string type;
    std::string dimensions;
    std::string rt60;
    std::string listener;
    std::string sourceDistance;
    std::string azimuthRange;
    std::string elevationRange;
    std::string measurementConfig;
};
std::vector<RoomInfoData> getRoomInfos();

#endif //AUDIO_FLOW_H
