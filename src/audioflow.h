#ifndef AUDIO_FLOW_H
#define AUDIO_FLOW_H

bool initialize(const std::string& configPath);
void cleanup();

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
void setEqualizerBand(int index, float f, float q, float g);
void setBufferSize(int newBufSize);

#endif //AUDIO_FLOW_H
