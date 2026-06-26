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

    bufferSize = 4096;

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

    int bufferSize = data.contains("bufferSize") ? data["bufferSize"].get<int>() : 4096;

    if (ampToggle != this->ampToggle || ampGain != this->ampGain || equalizerToggle != this->equalizerToggle || equalizerF != this->equalizerF || equalizerQ != this->equalizerQ || equalizerG != this->equalizerG || reverbToggle != this->reverbToggle || reverbDryWet != this->reverbDryWet || irFilePath != this->irFilePath || correctionToggle != this->correctionToggle || correctionDryWet != this->correctionDryWet || correctionIRFilePath != this->correctionIRFilePath || correctionPostGain != this->correctionPostGain || bufferSize != this->bufferSize) {
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

        this->bufferSize = bufferSize;

        return false;
    }

    return true;
}
