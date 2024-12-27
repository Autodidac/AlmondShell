#pragma once 

#include "alsEntryPoint.h"
#include "alsUtilities.h"
#include "alsExports_DLL.h"

#ifndef __linux__

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

#endif