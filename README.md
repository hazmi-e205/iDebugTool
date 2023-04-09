# iDebugTool (IOS Debugging Tool Cross-platform)
> **Supported OS:** Windows and Linux (64 bit)
![Nightly Workflow](https://github.com/hazmi-e205/iDebugTool/actions/workflows/windows_nightly.yml/badge.svg)

## Short Description
Rework of [iOS Debug Tool](https://github.com/hazmi-e205/IOS-Debug-Tool) that written in C++ with Qt Framework. This tool is wrapper of iMobileDevice and maybe we'll be add more useful IOS development tool that can work cross platform here. Inspired from Console app on MacOS for debugging Apple Development, why don't we bring them to "isekai" since more of app developer out there working remotely (using MacOS device just for build machine).

## Runtime Requirements
- iTunes (For Windows only)

## Features
- Connect iPhone via USB and socket (such as STF / Device farmer use case)
- Filter and Exclude logcat by text and regex
- Filter logcat by process id (it also text and regex support)
- Save logcat to file
- Install and Uninstall apps
- Show system and apps information
- Developer Disk Image mounter with 2 repositories options or offline mode using local files.
- Screenshot (required Developer Disk Image mounted)
- Restart, Shutdown, and Sleep the device
- Symbolicate Crashlogs using DWARF file or dSYM directory
- Re-codesign apps using P12 / PEM Private Key and Provision Profile

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

### Alternative
If you don't have Qt, I already add aqtinstaller command to `make.py` script but make sure your Python 3 have pip.
```
python3 .\Scripts\make.py --build --aqt=6.5.0
```
You can change `6.5.0` to Qt version that you need. This script will be do several tasks.
- Install Qt with mingw (very minimal modules)
- Build the project using Qt which installed by this script
- Deploy it, so it will be distributable

### Archive the build
This script will be archive the build to local repository directory.

- Nightly, file name example `iDebugTool-nightly-20230409144928-qt6.5.0-mingw11.2.0-win64.zip`
```
python3 .\Scripts\make.py --archive
```
- Release, file name example `iDebugTool-v2.0.0-alpha6-qt6.5.0-mingw11.2.0-win64.zip`
```
python3 .\Scripts\make.py --archive=release
```

## Download
Latest release available on [Release Page](https://github.com/hazmi-e205/iDebugTool/releases).
