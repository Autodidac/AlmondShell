#pragma once 

#include "EntryPoint.h"
#include "Utilities.h"
#include "Exports_DLL.h"

namespace almond {
    class ALMONDSHELL_API HeadlessEntryPoint : public EntryPoint {
    public:
       // void createEntryPoint(int width, int height, const wchar_t* title) override {}

        void show() override {}

        bool pollEvents() override {
            return false;
        }
    };
} // namespace almond
