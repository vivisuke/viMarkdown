# viMarkdown

-- visual Markdown editor --

**viMarkdown** is a lightweight, high-performance Markdown editor built with **Qt6** and **C++**.  
It is designed for developers and writers who love efficiency.

<img width="500" alt="image" src="./screen.png" />


## Features

- **Fast & Native:** Built with Qt6 for lightning-fast performance compared to Electron-based editors.
- **Tree View Navigation:** Manage your files and outline structure easily.
- **Tabbed Interface:** Edit multiple files simultaneously.
- **Markdown Preview:** Live preview of your Markdown content (supports Mermaid, MathJax).

## How to build

### Windows (Recommended)
The project is currently developed and tested on **Windows 11** using **Visual Studio 2026** with **Qt VS Tools**.

**Prerequisites:**
- **Visual Studio 2026** or later
- **Qt VS Tools** extension
- **Qt 6 SDK** (MSVC build)

**Build Instructions:**
1. Clone the repository.
2. Open `viMarkdown.sln` in Visual Studio.
3. If prompted, configure the Qt Version in the Qt VS Tools settings to match your installed Qt SDK.
4. **Build Solution** (Ctrl+Shift+B).

### Linux / macOS / Qt Creator
**Help Wanted:**
Currently, build verification on Linux, macOS, or using Qt Creator (via `.pro` or `CMakeLists.txt`) is **untested**.
If you are using these environments, you may need to generate the project files manually. Contributions to add `CMake` or `qmake` support are highly welcome!