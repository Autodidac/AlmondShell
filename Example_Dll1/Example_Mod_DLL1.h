#pragma once

#include "AlmondShell.h"
#include "Exports_DLL.h"

namespace almond {
    class ExampleMod : public Plugin {
    public:
        void Initialize(AlmondShell* mod) override;
        void Update(float deltaTime) override;
        void Shutdown() override;
        std::string Version() const override { return "0.0.1"; }

    private:
        AlmondShell* mod;
    };
}