//
// Created by Jeremi Campagna on 2024-07-30.
//

#include "config.h"
#include "iostream"

using json = nlohmann::json;

Config::Config(const std::string& configPath) : configFilePath(configPath) {
    ampToggle = false;
    ampGain = 0.0f;

    equalizerToggle = false;
    equalizerF = {60.0f, 170.0f, 310.0f, 600.0f, 1000.0f, 3000.0f, 6000.0f, 12000.0f, 14000.0f, 16000.0f};
    equalizerQ = {1.41f, 1.41f, 1.41f, 1.41f, 1.41f, 1.41f, 1.41f, 1.41f, 1.41f, 1.41f};
    equalizerG = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    reverbToggle = false;
    reverbDryWet = 0.0f;
    irFilePath = "";

    correctionToggle = false;
    correctionIRFilePath = "";
    correctionPostGain = 0.0f;
    correctionRecent = {};

    bufferSize = 4096;

    uiExpandedCorrecting = false;
    uiExpandedPreamplifier = false;
    uiExpandedEqualizer = false;
    uiExpandedReverb = false;
    uiExpandedSettings = false;

    loadConfig();
}

bool Config::loadConfig() {
    std::ifstream f(configFilePath);
    if (!f.is_open()) {
        std::cerr << "[Config] Cannot open config file: " << configFilePath << std::endl;
        return true;
    }
    std::cerr << "[Config] Loaded config from: " << configFilePath << std::endl;

    json data = json::parse(f);

    bool ampToggle = data.at("amplifier").at("toggle").get<bool>();
    float ampGain = data.at("amplifier").at("g").get<float>();

    bool equalizerToggle = data.at("equalizer").at("toggle").get<bool>();
    std::vector<float> equalizerF = data.at("equalizer").at("f").get<std::vector<float>>();
    std::vector<float> equalizerQ = data.at("equalizer").at("q").get<std::vector<float>>();
    std::vector<float> equalizerG = data.at("equalizer").at("g").get<std::vector<float>>();

    bool reverbToggle = data.at("reverb").at("toggle").get<bool>();
    float reverbDryWet = data.at("reverb").at("dw").get<float>();
    std::string irFilePath = data.at("reverb").at("ir").get<std::string>();

    bool correctionToggle = data.contains("correction") && data["correction"].contains("toggle") ? data["correction"]["toggle"].get<bool>() : false;
    float correctionDryWet = data.contains("correction") && data["correction"].contains("dw") ? data["correction"]["dw"].get<float>() : 1.0f;
    std::string correctionIRFilePath = data.contains("correction") && data["correction"].contains("ir") ? data["correction"]["ir"].get<std::string>() : "";
    float correctionPostGain = data.contains("correction") && data["correction"].contains("postGain") ? data["correction"]["postGain"].get<float>() : 0.0f;
    std::vector<std::string> correctionRecent = data.contains("correction") && data["correction"].contains("recent") ? data["correction"]["recent"].get<std::vector<std::string>>() : std::vector<std::string>{};

    int bufferSize = data.contains("bufferSize") ? data["bufferSize"].get<int>() : 4096;

    bool uiExpandedCorrecting = data.contains("ui") && data["ui"].contains("expanded") && data["ui"]["expanded"].contains("correcting") ? data["ui"]["expanded"]["correcting"].get<bool>() : false;
    bool uiExpandedPreamplifier = data.contains("ui") && data["ui"].contains("expanded") && data["ui"]["expanded"].contains("preamplifier") ? data["ui"]["expanded"]["preamplifier"].get<bool>() : false;
    bool uiExpandedEqualizer = data.contains("ui") && data["ui"].contains("expanded") && data["ui"]["expanded"].contains("equalizer") ? data["ui"]["expanded"]["equalizer"].get<bool>() : false;
    bool uiExpandedReverb = data.contains("ui") && data["ui"].contains("expanded") && data["ui"]["expanded"].contains("reverb") ? data["ui"]["expanded"]["reverb"].get<bool>() : false;
    bool uiExpandedSettings = data.contains("ui") && data["ui"].contains("expanded") && data["ui"]["expanded"].contains("settings") ? data["ui"]["expanded"]["settings"].get<bool>() : false;

    if (ampToggle != this->ampToggle || ampGain != this->ampGain || equalizerToggle != this->equalizerToggle || equalizerF != this->equalizerF || equalizerQ != this->equalizerQ || equalizerG != this->equalizerG || reverbToggle != this->reverbToggle || reverbDryWet != this->reverbDryWet || irFilePath != this->irFilePath || correctionToggle != this->correctionToggle || correctionDryWet != this->correctionDryWet || correctionIRFilePath != this->correctionIRFilePath || correctionPostGain != this->correctionPostGain || bufferSize != this->bufferSize || uiExpandedCorrecting != this->uiExpandedCorrecting || uiExpandedPreamplifier != this->uiExpandedPreamplifier || uiExpandedEqualizer != this->uiExpandedEqualizer || uiExpandedReverb != this->uiExpandedReverb || uiExpandedSettings != this->uiExpandedSettings || correctionRecent != this->correctionRecent) {
        this->ampToggle = ampToggle;
        this->ampGain = ampGain;

        this->equalizerToggle = equalizerToggle;
        this->equalizerF = equalizerF;
        this->equalizerQ = equalizerQ;
        this->equalizerG = equalizerG;

        this->reverbToggle = reverbToggle;
        this->reverbDryWet = reverbDryWet;
        this->irFilePath = irFilePath;

        this->correctionToggle = correctionToggle;
        this->correctionDryWet = correctionDryWet;
        this->correctionIRFilePath = correctionIRFilePath;
        this->correctionPostGain = correctionPostGain;
        this->correctionRecent = correctionRecent;

        this->bufferSize = bufferSize;

        this->uiExpandedCorrecting = uiExpandedCorrecting;
        this->uiExpandedPreamplifier = uiExpandedPreamplifier;
        this->uiExpandedEqualizer = uiExpandedEqualizer;
        this->uiExpandedReverb = uiExpandedReverb;
        this->uiExpandedSettings = uiExpandedSettings;

        return false;
    }

    return true;
}

bool Config::saveConfig() {
    json data = {
        {"amplifier", {
            {"toggle", ampToggle},
            {"g", ampGain}
        }},
        {"equalizer", {
            {"toggle", equalizerToggle},
            {"f", equalizerF},
            {"q", equalizerQ},
            {"g", equalizerG}
        }},
        {"reverb", {
            {"toggle", reverbToggle},
            {"dw", reverbDryWet},
            {"ir", irFilePath}
        }},
        {"correction", {
            {"toggle", correctionToggle},
            {"dw", correctionDryWet},
            {"ir", correctionIRFilePath},
            {"postGain", correctionPostGain},
            {"recent", correctionRecent}
        }},
        {"bufferSize", bufferSize},
        {"ui", {
            {"expanded", {
                {"correcting", uiExpandedCorrecting},
                {"preamplifier", uiExpandedPreamplifier},
                {"equalizer", uiExpandedEqualizer},
                {"reverb", uiExpandedReverb},
                {"settings", uiExpandedSettings}
            }}
        }}
    };

    std::ofstream f(configFilePath);
    if (!f.is_open()) {
        std::cerr << "[Config] Cannot write config file: " << configFilePath << std::endl;
        return false;
    }
    f << data.dump(4);
    std::cerr << "[Config] Saved config to: " << configFilePath << std::endl;
    return true;
}
