#ifndef CALIBRATION_HPP
#define CALIBRATION_HPP

extern "C" {
#include "servo.h"
}

#include <stdio.h>
#include <ctype.h>
#include <vector>
#include <fstream>
#include <string>
#include <iostream>

// This will be used to capture the feedback positions of the servos
// and save them to a file so they can be read back when needed
void calibrateLocations();
void printMenu();
short processInput(char);
void loadLocations();
void saveLocations();
void removeLocation();
void printLocations();
void captureLocation();

static std::vector<std::vector<int> > locations;

static const char* location_file_path = "../locations/locations.txt";

#endif