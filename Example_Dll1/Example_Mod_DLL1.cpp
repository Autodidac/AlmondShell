
//#include <SDL3>
#include "Example_Mod_DLL1.h"

namespace almond {
    void ExampleMod::Initialize(AlmondShell* mod) {
        this->mod = mod;
        mod->PrintMessage("ExampleMod initialized!");
    }

    void ExampleMod::Update(float deltaTime) {
        mod->PrintMessage("ExampleMod updating...");
    }

    void ExampleMod::Shutdown() {
        mod->PrintMessage("ExampleMod shutting down...");
    }

    extern "C" ENTRYPOINTLIBRARY_API Plugin * CreateMod() {
        return new ExampleMod();
    }

    extern "C" ENTRYPOINTLIBRARY_API void DestroyMod(Plugin * plugin) {
        delete plugin;
    }

//int Version() const override { return 1; }
}