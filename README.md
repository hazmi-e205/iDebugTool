# iDebugTool (IOS Debugging Tool Cross-platform)
> **Supported OS:** Windows and Linux (64 bit)

[![CircleCI](https://dl.circleci.com/insights-snapshot/gh/hazmi-e205/iDebugTool/main/Nightly/badge.svg?window=30d)](https://app.circleci.com/insights/github/hazmi-e205/iDebugTool/workflows/Nightly/overview?branch=main&reporting-window=last-30-days&insights-snapshot=true)

[![CircleCI](https://circleci.com/gh/hazmi-e205/iDebugTool/tree/main.svg?style=svg)](https://circleci.com/gh/hazmi-e205/iDebugTool/?branch=main)

## Short Description
Rework of [iOS Debug Tool](https://github.com/hazmi-e205/IOS-Debug-Tool) that written in C++ with Qt Framework. This tool is wrapper of iMobileDevice and maybe we'll be add more useful IOS development tool that can work cross platform here. Inspired from Console app on MacOS for debugging Apple Development, why don't we bring them to "isekai" since more of app developer out there working remotely (using MacOS device just for build machine).

## Runtime Requirements
- iTunes (For Windows)
- usbmuxd (For Linux)

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
- File Manager (Fetch, Download, Upload, and Delete files) (WIP)

## Build Steps
### Prerequisite
- Qt 6.5.3 with MinGW 11.2.0
- Python 3.8 or higher

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
python3 .\Scripts\make.py --build --aqt=6.5.3
```
You can change `6.5.3` to Qt version that you need. This script will be do several tasks.
- Install Qt with mingw (very minimal modules)
- Build the project using Qt which installed by this script
- Deploy it, so it will be distributable

Or you can install and open Qt Creator to open the project by call this command.
```
python3 .\Scripts\make.py --aqt=6.5.3ide
```
For Ubuntu user, you will required to install `sudo apt install python3-pip`. In the time I wrote this, `build-essential` will be installed when install pip, if it didn't installed yet please install it first before compiling the project.

### Archive the build
This script will be archive the build to local repository directory.

- Nightly, file name example `iDebugTool-nightly-20230409144928-qt6.5.3-mingw11.2.0-win64.zip`
```
python3 .\Scripts\make.py --archive
```
- Release, file name example `iDebugTool-v2.0.0-alpha6-qt6.5.3-mingw11.2.0-win64.zip`
```
python3 .\Scripts\make.py --archive=release
```

## Download
Latest release available on [Release Page](https://github.com/hazmi-e205/iDebugTool/releases).
