#pragma once

#include "ContextFunctions.h"
#include "EngineConfig.h"

#include <iostream>
#include <stdexcept>

namespace almond {

	void initSDL3();
	bool processSDL3();
	void cleanupSDL3();

}