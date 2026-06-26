//
// Created by Jeremi Campagna on 2024-07-30.
//

#ifndef EQ_CPP_CONFIG_H
#define EQ_CPP_CONFIG_H

#include "../lib/json.hpp"
#include <fstream>

class Config {
public:
    Config(const std::string& configPath = "../config.json");

    bool loadConfig();

    std::string configFilePath;

    bool ampToggle;
    float ampGain;

    bool equalizerToggle;
    std::vector<float> equalizerF;
    std::vector<float> equalizerQ;
    std::vector<float> equalizerG;

    bool reverbToggle;
    float reverbDryWet;
    std::string irFilePath;

    bool correctionToggle;
    float correctionDryWet;
    std::string correctionIRFilePath;
    float correctionPostGain;

    int bufferSize;
};


#endif //EQ_CPP_CONFIG_H
