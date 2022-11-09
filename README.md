# iDebugTool (IOS Debugging Tool Cross-platform)
> **Supported OS:** Windows and Linux (64 bit)

## Short Description
Rework of [iOS Debug Tool](https://github.com/hazmi-e205/IOS-Debug-Tool) that written in C++ with Qt Framework. This tool is wrapper of iMobileDevice and maybe we'll be add more useful IOS development tool that can work cross platform here. Inspired from Console app on MacOS for debugging Apple Development, why don't we bring them to "isekai" since more of app developer out there working remotely (using MacOS device just for build machine).

## Runtime Requirements
- iTunes (For Windows only)

## Build Steps
### Prerequisite
- Qt >= 5 with MinGW Compiler
- Python 3

### Setup
```
python3 .\Scripts\make.py --checkout --patch --premake
```
This command will does several things that you can disable it optionally.
- `--checkout`, clone all dependency repositories.
- `--patch`, apply patches and copy additional files.
- `--premake`, generate Qt projects using Premake, Premake binary will be downloaded once automatically.

### Build
Project will be generated in `Prj` folder. There is 2 projects for each platforms.
- `Prj/Qt-linux/iDebugTool.pro` for Linux project.
- `Prj/Qt-windows/iDebugTool.pro` for Windows project.

You can build using Qt Creator or command line.

## Download
Latest release available on [Release Page](https://github.com/hazmi-e205/iDebugTool/releases).
