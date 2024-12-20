#pragma once

#include <iostream>

int GetMajor();
int GetMinor();
int GetRevision();
// Version information
const int major = 0;
const int minor = 0;
const int revision = 9;
static char version_string[32] = "";

int GetMajor() { return major; }
int GetMinor() { return minor; }
int GetRevision() { return revision; }

extern "C" const char* GetEngineVersion()
{
    std::snprintf(version_string, sizeof(version_string), "%d.%d.%d", major, minor, revision);
    return version_string;
}
/*
#include "almondversion.h"

#include <iostream>

// Version information
const int major = 0;
const int minor = 0;
const int revision = 7;
static char version_string[32] = "";

int GetMajor() { return major; }
int GetMinor() { return minor; }
int GetRevision() { return revision; }

// Version string getter
const char* GetEngineVersion() {
    std::snprintf(version_string, sizeof(version_string), "%d.%d.%d", major, minor, revision);
    return version_string;
}

*/