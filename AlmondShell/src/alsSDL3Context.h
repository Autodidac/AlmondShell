#pragma once

#include "alsContextFunctions.h"
#include "alsEngineConfig.h"

#include <iostream>
#include <stdexcept>

namespace almond {

	void initSDL3();
	bool processSDL3();
	void cleanupSDL3();

}