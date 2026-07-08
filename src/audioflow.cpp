#include <algorithm>
#include <queue>
#include <functional>
#include <future>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudio.h>
#include <iostream>
#include <map>
#include <csignal>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "audioflow.h"
#include "permission_request.h"
#include "processing.h"
#include "./fileutils/globals.h"
#include "./lib/json.hpp"

using json = nlohmann::json;

UInt32 driverID;
UInt32 defaultDeviceID;
float deviceSampleRate;

AudioDeviceIOProcID inputIOProcId;
AudioDeviceIOProcID outputIOProcID;

std::vector<float> inputBuffer;
std::vector<float> processBuffer;
std::vector<float> outputBuffer;
std::mutex inputMutex;
std::mutex processMutex;

std::thread audioWorkerThread;
std::condition_variable processCV;

std::unique_ptr<Processing> audioProcessor;
std::mutex audioProcessorMutex;

Config* gConfig = nullptr;

std::atomic<bool> running{true};

std::map<UInt32 , std::string> getAudioDevices() {
    AudioObjectPropertyAddress propAddress;
    propAddress.mSelector = kAudioHardwarePropertyDevices;
    propAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propAddress.mElement = kAudioObjectPropertyElementMain;

    UInt32 propSize;
    OSStatus status = AudioObjectGetPropertyDataSize(
            kAudioObjectSystemObject, &propAddress, 0, nullptr, &propSize);

    if (status != noErr) {
        std::cerr << "Error getting device list size." << std::endl;
        return std::map<unsigned int, std::string>{};
    }

    AudioDeviceID* devices = new AudioDeviceID[propSize / sizeof(AudioDeviceID)];

    status = AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &propAddress, 0, nullptr, &propSize, devices);

    if (status != noErr) {
        std::cerr << "Error getting device list." << std::endl;
        delete[] devices;
        return std::map<unsigned int, std::string>{};
    }

    std::map<AudioDeviceID, std::string> m;
    for (UInt32 i = 0; i < propSize / sizeof(AudioDeviceID); ++i) {
        CFStringRef cfName;
        propAddress.mSelector = kAudioDevicePropertyDeviceName;
        propAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propAddress.mElement = kAudioObjectPropertyElementMain;

        UInt32 size = sizeof(CFStringRef);
        status = AudioObjectGetPropertyData(
                devices[i], &propAddress, 0, nullptr, &size, &cfName);

        if (status != noErr) {
            std::cerr << "Error getting device list." << std::endl;
            delete[] devices;
            return std::map<unsigned int, std::string>{};
        }

        AudioDeviceID deviceId = devices[i];
        char deviceName[256];

        propAddress.mSelector = kAudioObjectPropertyName;
        propAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propAddress.mElement = kAudioObjectPropertyElementMain;

        UInt32 dataSize = sizeof(CFStringRef);

        status = AudioObjectGetPropertyData(
                deviceId,
                &propAddress,
                0,
                nullptr,
                &dataSize,
                &cfName
        );

        if (status != noErr) {
            std::cerr << "Error getting device name: " << status << std::endl;
            return std::map<unsigned int, std::string>{};
        }

        CFStringGetCString(cfName, deviceName, sizeof(deviceName), kCFStringEncodingUTF8);
        CFRelease(cfName);

        m[deviceId] = deviceName;
    }

    delete[] devices;
    return m;
}

UInt32 getDefaultOutputDevice() {
    OSStatus status;
    AudioObjectPropertyAddress propertyAddress;
    AudioObjectID deviceID;

    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    UInt32 dataSize = sizeof(deviceID);
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &dataSize, &deviceID);
    if (status != noErr) {
        std::cerr << "Error getting default output device ID." << std::endl;
        return 0; // Return 0 or another appropriate error value
    }

    return deviceID;
}

bool setDefaultSystemOutputDevice(UInt32 deviceID) {
    OSStatus status;
    AudioObjectPropertyAddress propertyAddress;

    propertyAddress.mSelector = kAudioHardwarePropertyDefaultSystemOutputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    UInt32 dataSize = sizeof(deviceID);
    status = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, dataSize, &deviceID);
    if (status != noErr) {
        std::cerr << "Error setting default system output device ID: " << deviceID << std::endl;
        return false;
    }

    return true;
}

bool setDefaultOutputDevice(UInt32 deviceID) {
    OSStatus status;
    AudioObjectPropertyAddress propertyAddress;

    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    UInt32 dataSize = sizeof(deviceID);
    status = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, dataSize, &deviceID);
    if (status != noErr) {
        std::cerr << "Error setting default output device ID: " << deviceID << std::endl;
        return false;
    }

    return true;
}

bool setAudioDeviceBufferSize(AudioDeviceID deviceID, UInt32 bufferSizeInFrames) {
    OSStatus status;
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    status = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(UInt32), &bufferSizeInFrames);
    if (status != noErr) {
        std::cerr << "Error setting buffer size for device: " << deviceID << " Status: " << status << std::endl;
        return false;
    }

    return true;
}

float getAudioDeviceVolume(UInt32 deviceID) {
    AudioObjectPropertyAddress volumeAddress;
    volumeAddress.mSelector = kAudioDevicePropertyVolumeScalar;
    volumeAddress.mScope = kAudioDevicePropertyScopeOutput;
    volumeAddress.mElement = kAudioObjectPropertyElementMain;

    if (!AudioObjectHasProperty(deviceID, &volumeAddress)) {
        std::cerr << "Device " << deviceID << " does not have volume property" << std::endl;
        return -1.0;
    }

    float volume;
    UInt32 dataSize = sizeof(float);
    OSStatus status = AudioObjectGetPropertyData(deviceID, &volumeAddress, 0, nullptr, &dataSize, &volume);
    if (status != noErr) {
        std::cerr << "Error getting volume for device: " << deviceID << " Status: " << status << std::endl;
        return -1.0;
    }

    return volume;
}

bool setAudioDeviceVolume(UInt32 deviceID, float volume) {
    OSStatus status;
    Boolean isSettable;

    AudioObjectPropertyAddress volumeAddress;
    volumeAddress.mSelector = kAudioDevicePropertyVolumeScalar;
    volumeAddress.mScope = kAudioDevicePropertyScopeOutput;
    volumeAddress.mElement = kAudioObjectPropertyElementMain;

    if (AudioObjectHasProperty(deviceID, &volumeAddress)) {
        isSettable = false;
        AudioObjectIsPropertySettable(deviceID, &volumeAddress, &isSettable);
        if (isSettable) {
            status = AudioObjectSetPropertyData(deviceID, &volumeAddress, 0, nullptr, sizeof(float), &volume);
            if (status != noErr) {
                std::cerr << "Error setting volume for device: " << deviceID << " Status: " << status << std::endl;
                return false;
            }
        } else {
            std::cerr << "Device " << deviceID << " does not support volume control" << std::endl;
        }
    } else {
        std::cerr << "Device " << deviceID << " does not have volume property" << std::endl;
    }

    UInt32 mute = floor(1 - volume);
    AudioObjectPropertyAddress muteAddress;
    muteAddress.mSelector = kAudioDevicePropertyMute;
    muteAddress.mScope = kAudioDevicePropertyScopeOutput;
    muteAddress.mElement = kAudioObjectPropertyElementMain;

    if (AudioObjectHasProperty(deviceID, &muteAddress)) {
        isSettable = false;
        AudioObjectIsPropertySettable(deviceID, &muteAddress, &isSettable);
        if (isSettable) {
            status = AudioObjectSetPropertyData(deviceID, &muteAddress, 0, nullptr, sizeof(UInt32), &mute);
            if (status != noErr) {
                std::cerr << "Error unmuting device: " << deviceID << " Status: " << status << std::endl;
                return false;
            }
        } else {
            std::cerr << "Device " << deviceID << " does not support mute control" << std::endl;
        }
    }

    return true;
}

float getAudioDeviceSampleRate(AudioObjectID deviceID) {
    AudioObjectPropertyAddress propAddress;
    propAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
    propAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propAddress.mElement = kAudioObjectPropertyElementMain;

    if (!AudioObjectHasProperty(deviceID, &propAddress)) {
        std::cerr << "Device " << deviceID << " does not have sample rate property" << std::endl;
        return -1.0;
    }

    Float64 sampleRate = 0;
    UInt32 dataSize = sizeof(Float64);
    OSStatus status = AudioObjectGetPropertyData(deviceID, &propAddress, 0, nullptr, &dataSize, &sampleRate);
    if (status != noErr) {
        std::cerr << "Error getting sample rate for device: " << deviceID << " Status: " << status << std::endl;
        return -1.0;
    }

    return static_cast<float>(sampleRate);
}

OSStatus driverIOProc(
        AudioObjectID inDevice,
        const AudioTimeStamp* inNow,
        const AudioBufferList* inInputData,
        const AudioTimeStamp* inInputTime,
        AudioBufferList* outOutputData,
        const AudioTimeStamp* inOutputTime,
        void* inClientData
);

OSStatus defaultDeviceIOProc(
        AudioObjectID inDevice,
        const AudioTimeStamp* inNow,
        const AudioBufferList* inInputData,
        const AudioTimeStamp* inInputTime,
        AudioBufferList* outOutputData,
        const AudioTimeStamp* inOutputTime,
        void* inClientData
);

const Config& getConfig() {
    return *gConfig;
}

std::string getCurrentOutputDeviceName() {
    AudioObjectPropertyAddress propAddress;
    propAddress.mSelector = kAudioObjectPropertyName;
    propAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propAddress.mElement = kAudioObjectPropertyElementMain;

    CFStringRef cfName;
    UInt32 size = sizeof(CFStringRef);
    OSStatus status = AudioObjectGetPropertyData(
            defaultDeviceID, &propAddress, 0, nullptr, &size, &cfName);
    if (status != noErr) {
        return "";
    }

    char name[256];
    CFStringGetCString(cfName, name, sizeof(name), kCFStringEncodingUTF8);
    CFRelease(cfName);
    return std::string(name);
}

std::vector<std::string> getAvailableOutputDevices() {
    std::vector<std::string> result;
    auto devices = getAudioDevices();

    for (auto const& [deviceID, name] : devices) {
        if (name != driver && name != driver2) {
	    AudioObjectPropertyAddress propAddress;
	    propAddress.mSelector = kAudioDevicePropertyStreams;
	    propAddress.mScope = kAudioObjectPropertyScopeOutput;
	    propAddress.mElement = kAudioObjectPropertyElementMain;

	    UInt32 dataSize = 0;
	    OSStatus status = AudioObjectGetPropertyDataSize(
							     deviceID, &propAddress, 0, nullptr, &dataSize);
	    if (status == noErr && dataSize > 0) {
		result.push_back(name);
	    }
        }
    }

    return result;
}

bool setOutputDevice(const std::string& name) {
    auto devices = getAudioDevices();
    UInt32 newDeviceID = 0;
    bool found = false;

    for (auto const& [id, deviceName] : devices) {
        if (deviceName == name) {
            newDeviceID = id;
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Device not found: " << name << std::endl;
        return false;
    }

    if (newDeviceID == defaultDeviceID) {
        return true;
    }

    float driverVolume = getAudioDeviceVolume(driverID);
    setAudioDeviceVolume(defaultDeviceID, driverVolume);

    AudioDeviceStop(driverID, inputIOProcId);
    AudioDeviceStop(defaultDeviceID, outputIOProcID);
    AudioDeviceDestroyIOProcID(driverID, inputIOProcId);
    AudioDeviceDestroyIOProcID(defaultDeviceID, outputIOProcID);

    {
        std::lock_guard<std::mutex> lock(inputMutex);
        inputBuffer.clear();
    }
    {
        std::lock_guard<std::mutex> lock(processMutex);
        processBuffer.clear();
        outputBuffer.clear();
    }

    defaultDeviceID = newDeviceID;
    setAudioDeviceVolume(defaultDeviceID, 1);

    UInt32 bufferSizeInFrames = bufferSize;
    if (!setAudioDeviceBufferSize(defaultDeviceID, bufferSizeInFrames)) {
        std::cerr << "Failed to set buffer size for default output device: " << defaultDeviceID << std::endl;
    }

    deviceSampleRate = getAudioDeviceSampleRate(defaultDeviceID);
    if (deviceSampleRate <= 0) {
        std::cerr << "Failed to get device sample rate, defaulting to 48000" << std::endl;
        deviceSampleRate = 48000.0f;
    }

    auto updated = std::make_unique<Processing>(*gConfig, getAudioDeviceVolume(driverID), deviceSampleRate);
    audioProcessorMutex.lock();
    audioProcessor = std::move(updated);
    audioProcessorMutex.unlock();

    // Create audio device processes
    AudioDeviceCreateIOProcID(driverID, driverIOProc, nullptr, &inputIOProcId);
    AudioDeviceCreateIOProcID(defaultDeviceID, defaultDeviceIOProc, nullptr, &outputIOProcID);

    // Open the audio device for input or output
    AudioDeviceStart(driverID, inputIOProcId);
    AudioDeviceStart(defaultDeviceID, outputIOProcID);

    return true;
}

void setReverbToggle(bool toggle) {
    audioProcessorMutex.lock();
    audioProcessor->setReverbToggle(toggle);
    audioProcessorMutex.unlock();
    gConfig->reverbToggle = toggle;
}

void setReverbDryWet(double dryWet) {
    audioProcessorMutex.lock();
    audioProcessor->setReverbDryWet(dryWet);
    audioProcessorMutex.unlock();
    gConfig->reverbDryWet = static_cast<float>(dryWet);
}

void setReverbIRFile(const std::string& path) {
    audioProcessorMutex.lock();
    audioProcessor->setReverbIRFile(path);
    audioProcessorMutex.unlock();
    gConfig->irFilePath = path;
}

void setCorrectionToggle(bool toggle) {
    audioProcessorMutex.lock();
    audioProcessor->setCorrectionToggle(toggle);
    audioProcessorMutex.unlock();
    gConfig->correctionToggle = toggle;
}

void setCorrectionIRFile(const std::string& path) {
    audioProcessorMutex.lock();
    audioProcessor->setCorrectionIRFile(path);
    audioProcessorMutex.unlock();

    gConfig->correctionIRFilePath = path;
    auto &recent = gConfig->correctionRecent;
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if (recent.size() > 5)
        recent.resize(5);
}

void setCorrectionDryWet(double dryWet) {
    audioProcessorMutex.lock();
    audioProcessor->setCorrectionDryWet(dryWet);
    audioProcessorMutex.unlock();
    gConfig->correctionDryWet = static_cast<float>(dryWet);
}

void setCorrectionPostGain(float postGain) {
    audioProcessorMutex.lock();
    audioProcessor->setCorrectionPostGain(postGain);
    audioProcessorMutex.unlock();
    gConfig->correctionPostGain = postGain;
}

void setEqualizerToggle(bool toggle) {
    audioProcessorMutex.lock();
    audioProcessor->setEqualizerToggle(toggle);
    audioProcessorMutex.unlock();
    gConfig->equalizerToggle = toggle;
}

void setAmplifierToggle(bool toggle) {
    audioProcessorMutex.lock();
    audioProcessor->setAmplifierToggle(toggle);
    audioProcessorMutex.unlock();
    gConfig->ampToggle = toggle;
}

void setAmplifierGain(float gain) {
    audioProcessorMutex.lock();
    audioProcessor->setAmplifierGain(gain);
    audioProcessorMutex.unlock();
    gConfig->ampGain = gain;
}

void setAmplifierAuto(bool enabled) {
    audioProcessorMutex.lock();
    audioProcessor->setAmplifierAuto(enabled);
    audioProcessorMutex.unlock();
    gConfig->ampAuto = enabled;
}

float getAmplifierAutoGainValue() {
    std::lock_guard<std::mutex> lock(audioProcessorMutex);
    float v = audioProcessor->getAmplifierAutoGainValue();
    gConfig->ampGain = v;
    return v;
}

void setEqualizerBand(int index, float f, float q, float g) {
    audioProcessorMutex.lock();
    audioProcessor->setEqualizerBand(index, f, q, g);
    audioProcessorMutex.unlock();
    if (index >= 0 && index < 10) {
        gConfig->equalizerF[static_cast<size_t>(index)] = f;
        gConfig->equalizerQ[static_cast<size_t>(index)] = q;
        gConfig->equalizerG[static_cast<size_t>(index)] = g;
    }
}

void setBufferSize(int newBufSize) {
    static const int allowed[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    bool valid = false;
    for (int v : allowed) { if (v == newBufSize) { valid = true; break; } }
    if (!valid) {
        std::cerr << "invalid buffer size " << newBufSize << std::endl;
    } else {
        AudioDeviceStop(driverID, inputIOProcId);
        AudioDeviceStop(defaultDeviceID, outputIOProcID);

        bufferSize = newBufSize;
        gConfig->bufferSize = newBufSize;
        setAudioDeviceBufferSize(driverID, newBufSize);
        setAudioDeviceBufferSize(defaultDeviceID, newBufSize);

        {
            std::lock_guard<std::mutex> lock(inputMutex);
            inputBuffer.clear();
        }
        {
            std::lock_guard<std::mutex> lock(processMutex);
            processBuffer.clear();
            outputBuffer.clear();
        }

        AudioDeviceStart(driverID, inputIOProcId);
        AudioDeviceStart(defaultDeviceID, outputIOProcID);
    }
}

float getLatencyMs() {
    return static_cast<float>(bufferSize) / deviceSampleRate * 1000.0f;
}

void setUIExpandedCorrecting(bool expanded) {
    gConfig->uiExpandedCorrecting = expanded;
}

void setUIExpandedPreamplifier(bool expanded) {
    gConfig->uiExpandedPreamplifier = expanded;
}

void setUIExpandedEqualizer(bool expanded) {
    gConfig->uiExpandedEqualizer = expanded;
}

void setUIExpandedReverb(bool expanded) {
    gConfig->uiExpandedReverb = expanded;
}

void setUIExpandedSettings(bool expanded) {
    gConfig->uiExpandedSettings = expanded;
}

void cleanup() {
    static std::atomic<bool> cleanedUp{false};
    if (cleanedUp.exchange(true)) return;

    running = false;
    processCV.notify_one();

    std::cerr << "[main] exiting main loop" << std::endl;

    if (audioWorkerThread.joinable()) {
        audioWorkerThread.join();
    }

    gConfig->saveConfig();

    float driverVolume = getAudioDeviceVolume(driverID);
    setAudioDeviceVolume(defaultDeviceID, driverVolume);
    setDefaultOutputDevice(defaultDeviceID);
    setDefaultSystemOutputDevice(defaultDeviceID);

    AudioDeviceStop(driverID, inputIOProcId);
    AudioDeviceStop(defaultDeviceID, outputIOProcID);

    AudioDeviceDestroyIOProcID(driverID, inputIOProcId);
    AudioDeviceDestroyIOProcID(defaultDeviceID, outputIOProcID);
    std::cerr << "[main] cleanup done" << std::endl;

}

void audioWorker() {
    while (running) {
        std::vector<float> chunk;
        {
            std::unique_lock<std::mutex> lock(inputMutex);
            processCV.wait_for(lock, std::chrono::milliseconds(100), [] {
                return !inputBuffer.empty() || !running;
            });
            if (!running) break;
            if (inputBuffer.empty()) continue;
            size_t chunkSize = static_cast<size_t>(2 * bufferSize);
            size_t toProcess = std::min(chunkSize, inputBuffer.size());
            chunk.assign(inputBuffer.begin(), inputBuffer.begin() + toProcess);
            inputBuffer.erase(inputBuffer.begin(), inputBuffer.begin() + toProcess);
        }

        audioProcessor->process(chunk);

        {
            std::lock_guard<std::mutex> lock(processMutex);
            outputBuffer.insert(outputBuffer.end(), chunk.begin(), chunk.end());

            size_t maxOutput = static_cast<size_t>(8 * bufferSize);
            if (outputBuffer.size() > maxOutput) {
                outputBuffer.erase(outputBuffer.begin(), outputBuffer.begin() + (outputBuffer.size() - maxOutput));
            }
        }
    }
}

OSStatus driverIOProc(
        AudioObjectID inDevice,
        const AudioTimeStamp* inNow,
        const AudioBufferList* inInputData,
        const AudioTimeStamp* inInputTime,
        AudioBufferList* outOutputData,
        const AudioTimeStamp* inOutputTime,
        void* inClientData
) {
    for (size_t i = 0; i < inInputData->mNumberBuffers; ++i) {
        AudioBuffer buffer = inInputData->mBuffers[i];
        float* audioData = (float*)buffer.mData;
        UInt32 numSamples = buffer.mDataByteSize / sizeof(float);

        {
            std::lock_guard<std::mutex> lock(inputMutex);
            inputBuffer.insert(inputBuffer.end(), audioData, audioData + numSamples);

            size_t maxInput = static_cast<size_t>(8 * bufferSize);
            if (inputBuffer.size() > maxInput) {
                inputBuffer.erase(inputBuffer.begin(), inputBuffer.begin() + (inputBuffer.size() - maxInput));
            }

            if (inputBuffer.size() >= static_cast<size_t>(2 * bufferSize)) {
                processCV.notify_one();
            }
        }
    }

    return noErr;
}

OSStatus defaultDeviceIOProc(
        AudioObjectID inDevice,
        const AudioTimeStamp* inNow,
        const AudioBufferList* inInputData,
        const AudioTimeStamp* inInputTime,
        AudioBufferList* outOutputData,
        const AudioTimeStamp* inOutputTime,
        void* inClientData
) {
    for (size_t i = 0; i < outOutputData->mNumberBuffers; ++i) {
        AudioBuffer outBuffer = outOutputData->mBuffers[i];
        float* outputData = (float*)outBuffer.mData;
        UInt32 numSamples = outBuffer.mDataByteSize / sizeof(float);

        std::lock_guard<std::mutex> lock(processMutex);
        size_t toCopy = std::min<size_t>(numSamples, outputBuffer.size());
        if (toCopy > 0) {
            std::copy(outputBuffer.begin(), outputBuffer.begin() + toCopy, outputData);
            outputBuffer.erase(outputBuffer.begin(), outputBuffer.begin() + toCopy);
        } else {
            std::memset(outputData, 0, outBuffer.mDataByteSize);
        }
    }

    return noErr;
}

bool initialize(const std::string& configPath) {
    driverID = 0;
    
    requestMicrophonePermission();
    
    // Get device IDs
    std::map<UInt32, std::string> ad = getAudioDevices();
    for (auto const& [key, val] : ad) {
        if (val == driver || val == driver2) {
	    driverID = key;
	}
    }
    if (driverID == 0) {
        // Driver not installed in OS. Fault.
        std::cerr << "Install Blackhole driver to your OS in order to use AudioFlow!";
        return false;
    }
    
    defaultDeviceID = getDefaultOutputDevice();

    // Volume and device swaps
    float defaultDeviceVolume = getAudioDeviceVolume(defaultDeviceID);
    if (defaultDeviceVolume > 0.0) {
        setAudioDeviceVolume(driverID, defaultDeviceVolume);
    } else {
        setAudioDeviceVolume(driverID, 1.0);
    }
    setDefaultOutputDevice(driverID);
    setDefaultSystemOutputDevice(driverID);
    setAudioDeviceVolume(defaultDeviceID, 1);

    // Get device sample rate
    deviceSampleRate = getAudioDeviceSampleRate(defaultDeviceID);
    if (deviceSampleRate == 0) {
        std::cerr << "Failed to get device sample rate, defaulting to 48000" << std::endl;
        deviceSampleRate = 48000.0f;
    }

    gConfig = new Config(configPath);
    bufferSize = gConfig->bufferSize;

    // Set buffer size
    UInt32 bufferSizeInFrames = bufferSize;
    if (!setAudioDeviceBufferSize(driverID, bufferSizeInFrames)) {
        std::cerr << "Failed to set buffer size for driver device: " << driverID << std::endl;
    }
    if (!setAudioDeviceBufferSize(defaultDeviceID, bufferSizeInFrames)) {
        std::cerr << "Failed to set buffer size for default output device: " << defaultDeviceID << std::endl;
    }

    audioProcessor = std::make_unique<Processing>(*gConfig, getAudioDeviceVolume(driverID), deviceSampleRate);
    audioProcessor = std::make_unique<Processing>(*gConfig, audioProcessor.get(), getAudioDeviceVolume(driverID), deviceSampleRate);

    // Create audio device processes
    AudioDeviceCreateIOProcID(driverID, driverIOProc, nullptr, &inputIOProcId);
    AudioDeviceCreateIOProcID(defaultDeviceID, defaultDeviceIOProc, nullptr, &outputIOProcID);

    // Open the audio device for input or output
    AudioDeviceStart(driverID, inputIOProcId);
    AudioDeviceStart(defaultDeviceID, outputIOProcID);

    audioWorkerThread = std::thread(audioWorker);

    return true;
}

