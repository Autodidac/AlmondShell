cmake_minimum_required(VERSION 3.20)
project(AlmondShell\examples\Example_CMake_Project1)

# Include shared items project
add_subdirectory(AlmondShell)

# Include static library project
add_subdirectory(AlmondShell\examples\Example_StaticLib1)

# Include DLL project
add_subdirectory(AlmondShell\examples\Example_Dll1)

# Include Windows/console example project
add_subdirectory(AlmondShell\examples\Example_WindowsProject1)

# Include plugin example project
add_subdirectory(AlmondShell\examples\Example_Mod_Dll1)

# Include Android example project
add_subdirectory(AlmondShell\examples\Example_Android_StaticLibrary1)
