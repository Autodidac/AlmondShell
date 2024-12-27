#pragma once

#include <iostream>

int GetMajor();
int GetMinor();
int GetRevision();
// Version information
const int major = 0;
const int minor = 1;
const int revision = 0;
static char version_string[32] = "";

int GetMajor() { return major; }
int GetMinor() { return minor; }
int GetRevision() { return revision; }

extern "C" const char* GetEngineVersion()
{
    std::snprintf(version_string, sizeof(version_string), "%d.%d.%d", major, minor, revision);
    return version_string;
}
