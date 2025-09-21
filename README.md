# Cpp_Ultimate_Project_Updater

# ⚠️ IMPORTANT WARNING AND NOTICE!

🚨 **Always run the updater in its own directory!** 🚨  
The updater **creates and modifies files in the directory where it runs**.  
Running it inside an existing project folder **may overwrite files**.  
**Recommended:** **Create a new directory where you want the updater to manage files.**

```cpp
/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for non-commercial purposes only.          *
 *   Redistribution allowed with this notice.                 *
 *   No obligation to disclose modifications.                 *
 *   See LICENSE file for full terms.                         *
 **************************************************************/
```

---

# C++ Ultimate Project Updater

A **fully automated, centralized, self-updating entry point** for projects.  
This updater **requires no manual cloning or forking**—it will **automatically download, update, and manage project files** while keeping itself updated.  

🚀 **No need to manage repositories manually**—just run the updater and let it handle everything!

## 🛠️ How to Use

### **🔹 Quick Start (Recommended)**
1. **Create a new directory where you want the updater to manage files.**  
   - ⚠️ **DO NOT run the updater in an existing project directory.** It will manage its own files!
2. **Download** `updater.exe` (Windows) or `updater` (Linux/macOS) from the **Releases** section ➜ _(On the right side of the GitHub page)_.
3. **Run the updater**:
   - **Windows**: Double-click `updater.exe` or run it from the command line.
   - **Linux/macOS**: Open a terminal in the directory and run:
     ```sh
     chmod +x updater && ./updater
     ```
4. The updater will **automatically fetch and manage all required project files** in the directory.
5. **Run the updater again** in the same directory whenever you want to check for updates.

---

## 📦 Normal Repository Instructions

For developers who want to manually build and modify the project:

### **🔹 Clone the Repository**
```sh
git clone https://github.com/Autodidac/Cpp_Ultimate_Project_Updater.git
cd Cpp_Ultimate_Project_Updater
```

### **🔹 Building from Source**
**Windows (MSVC + CMake)**
```sh
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

**Linux/macOS (Clang/GCC + Ninja)**
```sh
mkdir build
cd build
cmake .. -G Ninja
ninja
```

### **🔹 Running the Updater from Source**
After building, run:
```sh
./updater
```
or on Windows:
```sh
updater.exe
```

---

## 🔗 **Releases & Downloads**
Get the latest version of `updater.exe` or `updater` from the **[Releases](https://github.com/Autodidac/Cpp_Ultimate_Project_Updater/releases)** section.

For support or bug reports, open an **[issue](https://github.com/Autodidac/Cpp_Ultimate_Project_Updater/issues)**.

---

## ⚖️ Legal Information
📄 License
This project is licensed under the "SPDX-License-Identifier: LicenseRef-MIT-NoSell" License.
See LICENSE.md for full terms.

🛡️ Disclaimer of Warranty
This software is provided “AS IS”, without warranty of any kind, express or implied (including merchantability, fitness for a particular purpose, or non-infringement). Use at your own risk.

📌 Limitation of Liability
In no event shall the authors or copyright holders be liable for any claim, damages, or other liability—contract, tort, or otherwise—arising from, out of, or in connection with the software or its use.

🔗 Third-Party Components
Where applicable, this project may include third-party libraries. Each component remains under its own license—refer to its documentation or source headers.

🤝 Commercial Licensing
For a warranty, indemnification, or proprietary commercial license, contact legal@yourdomain.com.