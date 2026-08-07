//
// Created by Jeremi Campagna on 2024-08-28.
//

#ifndef EQ_CPP_GLOBALS_H
#define EQ_CPP_GLOBALS_H

#include <string>
#include "config.h"

extern std::string driver;
extern std::string driver2;
extern int bufferSize;
extern int smootherSteps;
extern int volumeSmootherSteps;
extern int convolutionChunkSize;
extern Config* gConfig;

#endif //EQ_CPP_GLOBALS_H
