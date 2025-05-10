[![wakatime](https://wakatime.com/badge/user/17eacdf5-0954-4233-89e5-cf09a2e230eb/project/d3d1992b-0b79-49fc-b226-34f025ec1ad9.svg)](https://wakatime.com/badge/user/17eacdf5-0954-4233-89e5-cf09a2e230eb/project/d3d1992b-0b79-49fc-b226-34f025ec1ad9)



<img align="left" src="AlmondShell/images/567.jpg" width="70px"/>

# Almond Shell

An Extension of Almond-Core featuring plugin support with 2D graphical support for C++20 Cross-platform UX/UI. Like Almond Core it's also Part of the Almond Engine and is a Cross-platform OS Entry-Point System with Data-Oriented, Functional/OOP Hybrid, Task and Event Driven Component based Architecture with built-in Stateless Scene Management, ECS, WITH THE ADDITION OF plugin and 2D Graphical Support + Custom Minimal UX/UI Implimentation.

What Seperates Almond Core From Almond Shell is plugin and 2D Graphical Support + Custom Minimal UX/UI Implimentation.

Built with contemporary design principles and using C++20, it is designed for modularity, efficiency, and ease of use on current OS hardware for bleeding-edge software development and beyond. For 3D Graphics Rendering Capabilities or Game Engine see AlmondEngine. AlmondShell is a Modern Software Development Suite which aims to be as minimal as possible and also features a header only mostly functional programming design to reduce overhead.

<img align="left" src="AlmondShell/images/gpt.jpg" width="40px"/>GPT-4



## Features


- Cross-platform support for Windows, macOS, Linux, Android, iOS, WebAssembly, PS5, Xbox, and a No-op/Console mode.
- Game Creation and Management.
- Platform-specific implementation details hidden behind a common interface.
- Namespace 'almondengine' Prevents Global Code Leaks
- Modern C++20 and Efficient Pattern Design
- Platform-Specific Factories
- Custom Efficient Memory Management and Logging, and Error Handling
- Split-Screen and VR Features
In AlmondEngine, Ensure that these features are appropriately implemented in the platform-specific classes where applicable.



## Tested

- Tested in Windows 10/11, and Linux Kubuntu (Ubuntu Debian/KDE)

<img align="left" src="AlmondShell/images/1.png" width="200px"/>

<img align="left" src="AlmondShell/images/4.png" width="200px"/>

```cpp



## Supported Platforms

- **Console** (Headless mode)
- **Windows** (Win32)
- **Linux** (XCB or XLib) (XCB planned, XLib Working!)

- **macOS** (Cocoa) (planned)
- **Android** (Android) (planned)
- **iOS** (UIKit) (planned)
- **WebAssembly** (Emscripten) (planned)
- **PlayStation 5** (PS5) (planned)
- **Xbox** (Xbox) (planned)

```

## Installation



### Prerequisites

- vcpkg (Windows and Linux)
- MSVC with C++20 or later, or VSCode, VSCodium on XLib
- CMake 3.10 or later




### Building

1. Clone the repository:
```sh
   git clone https://github.com/Autodidac/AlmondShell.git
   cd AlmondShell
```




### Build using WIN32 (Windows 11 64bit) and vs2022:
Simply run the provided vs solution file `AlmondShell.sln` in the main folder

### Build using WIN32 (Windows 11 64bit) and VSCode:
Simply open the top project folder containing `AlmondShell.sln` in VSCode or VSCodium on Linux


## CMake works through VSCodium on Linux, CMake is not used directly but should work.

### Generate build files using CMake:
Run cmake to generate the build files, then compile:





### Build the project:
Basic Build:
```sh
mkdir build
cd build
cmake ..
cmake --build .
```


### Usage


##### Example Console Batch
```batch
@echo off
Example_ConsoleApplication.exe 
pause
```
##### Example Console Bash
```bash
./Example_CMake_Project1 
```



##### Example - This is outdated until this part is finalized
```cpp
#include "AlmondShell.h"

int main() {
    auto AlmondShell = almond::renderer::createAlmondShell();
    if (AlmondShell->init()) {
        AlmondShell->createWindow(L"Example Game", 800, 600);
        // Main loop or additional logic here
        AlmondShell->shutdown();
    }
    return 0;
}
```


### API Reference

```cpp
almond::AlmondShell
static std::unique_ptr<AlmondShell> createAlmondShell()
bool init()
bool createWindow(const wchar_t* title, int width, int height)
void shutdown()

```



### Platform-specific Implementations
The platform-specific implementation files are organized into separate source files. Each implementation file contains the initialization, window creation, and shutdown code for a specific platform.

-Windows: `AlmondShellWin32.cpp` and `AlmondEngineWin32.h`

-macOS: `AlmondEngineMac.cpp` and `AlmondEngineMac.h`

-Linux: `AlmondEngineLinux.cpp` and `AlmondEngineLinux.h`

-Android: `AlmondEngineAndroid.cpp` and `AlmondEngineAndroid.h`

-iOS: `AlmondEngineIOS.cpp` and `AlmondEngineIOS.h`

-WebAssembly: `AlmondEngineWebAssembly.cpp` and `AlmondEngineWebAssembly.h`

-PlayStation 5: `AlmondEnginePS5.cpp` and `AlmondEnginePS5.h`

-Xbox: `AlmondEngineXbox.cpp` and `AlmondEngineXbox.h`

-No-op mode: `AlmondEngineConsole.cpp` and `AlmondEngineConsole.h`





### License
AlmondShell is licensed under the SPDX-License-Identifier: LicenseRef-MIT-NoSell license
See the LICENSE file for details. - **[Download LICENSE](https://github.com/Autodidac/AlmondShell/raw/main/LICENSE)**




### Contributing
Contributions are welcome! Please see the CONTRIBUTING.md file for guidelines.




## Acknowledgements
This project was developed with the support of OpenAI's GPT-4, which provided valuable assistance in generating code and design ideas. 

I appreciate the power and capabilities of GPT-4 in aiding the creation of this project and others.



<img align="left" src="AlmondShell/images/AlmondShell.jpg" width="80px"/>
<img align="center" src="AlmondShell/images/gpt4all.png" width="80px"/>

Almond Shell - A GPT-4 Cross-platform Library For All! Almost Entirely Built and Designed by AI - Even much of this readme and the CMakeLists.txt are written by GPT-4.. what a wonderful tool Thank You Again!

