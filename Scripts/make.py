from importlib.metadata import version
import sys
import os
import json
import re
from concurrent.futures import ThreadPoolExecutor
from urllib.request import urlopen
import tarfile, zipfile
from io import BytesIO
import datetime

import utils
import colorama
from termcolor import cprint


script_dir      = os.path.dirname(os.path.abspath(__file__))
base_dir        = os.path.abspath(script_dir + "/../")
prj_dir         = os.path.abspath(base_dir + "/Prj/")
build_dir       = os.path.abspath(base_dir + "/Build/")
external_dir    = os.path.abspath(base_dir + "/Externals/")
patch_dir       = os.path.abspath(base_dir + "/Externals/_Patches/")
info_path       = os.path.abspath(base_dir + "/info.json")
exe_ext         = ".exe" if sys.platform == "win32" or sys.platform == "cygwin" or sys.platform == "msys" else ""
premake_path    = os.path.abspath(script_dir + "/premake5") + exe_ext
premake_tag     = "v5.0.0-beta1"

#compiler
qt_dir           = "C:/Qt"
qt_version       = "6.5.1"
aqt_compiler     = "tools_mingw90"
aqt_creator      = "qtcreator_gui"
compiler_name    = "gcc" if "linux" in sys.platform else "mingw"
compiler_version = "11.2.0"
compiler_arch    = "64"
platform_var     = "win"
prj_name         = "iDebugTool"
prj_type         = "Qt-linux" if "linux" in sys.platform else "Qt-windows"

#flags
is_checkout     = False
is_deprecated   = False
is_reset        = False
is_premake      = False
is_apply_patch  = False
is_create_patch = False
is_build        = False
is_debug        = False
is_aqtinstaller = False
is_aqtcreator   = False
is_archive      = False
is_nightly      = True
release_tag     = ""


def DownloadPremake():
    request_releases = urlopen("https://api.github.com/repos/Premake/premake-core/releases").read()
    premake_releases = json.loads(request_releases)
    filtered = [release for release in premake_releases if release['tag_name'] == premake_tag]
    if len(filtered) == 0:
        sys.exit("Release with tag name '{}' not found!".format(premake_tag))
    premake_latest   = filtered[0]["assets"]
    premake_download = False
    for asset in premake_latest:
        if "windows" in asset["name"] and (sys.platform == "win32" or sys.platform == "cygwin" or sys.platform == "msys"):
            premake_download = True
        elif "linux" in asset["name"] and (sys.platform == "linux" or sys.platform == "linux2"):
            premake_download = True
        elif "macosx" in asset["name"] and sys.platform == "darwin":
            premake_download = True

        if premake_download == True:
            cprint("Download `" + asset["name"] + "`...", 'magenta', attrs=['reverse', 'blink'])
            premake_file = urlopen(asset["browser_download_url"]).read()
            if ".tar.gz" in asset["name"]:
                t = tarfile.open(name=None, fileobj=BytesIO(premake_file))
                t.extractall(script_dir)
                t.close()
            if ".zip" in asset["name"]:
                z = zipfile.ZipFile(BytesIO(premake_file))
                z.extractall(script_dir)
                z.close()
            cprint("Premake downloaded!", 'magenta', attrs=['reverse', 'blink'])
            break


def LoadInfo():
    global project_info
    project_info = {}
    if os.path.exists(info_path):
        mInfoFile = open(info_path, "r")
        project_info = json.load(mInfoFile)
    else:
        sys.exit("Hey, info.json should be exist!")


def SaveRevisionInfo(project_name):
    for repo_item in project_info["dependencies"]:
        if repo_item["project"].lower() == project_name.lower():
            repo_path = os.path.abspath(base_dir + "/" + repo_item["path"])
            repo_item["revision"] = utils.Git.get_revision(repo_path)
            break
            
    with open(info_path, "w") as outfile:
        json.dump(project_info, outfile, indent=4)


def UpdateInfoFromTag(tag_name):
    tag_name = tag_name.strip()
    if tag_name.startswith("refs/tags/"):
        tag_name = tag_name.removeprefix("refs/tags/")
    if tag_name == "":
        return

    m = re.match(r"^(?P<version>\d+\.\d+\.\d+)(?:-(?P<status>[A-Za-z0-9._-]+))?$", tag_name)
    if m is None:
        sys.exit("Invalid release tag '{}'. Expected format: <major>.<minor>.<patch> or <major>.<minor>.<patch>-<status>".format(tag_name))

    project_info["version"] = m.group("version")
    project_info["status"] = m.group("status") if m.group("status") else "release"

    with open(info_path, "w") as outfile:
        json.dump(project_info, outfile, indent=4)


def UpdateExternal(repoItem):
    repo_str = "(" + repoItem["project"] + " Branch:" + repoItem["branch"] + ")\t| "
    external_path = os.path.abspath(base_dir + "/" + repoItem["path"])
    revision = repoItem["revision"] if ("revision" in repoItem and repoItem["revision"] != None) else ""
    use_submodules = repoItem["use_submodules"] if ("use_submodules" in repoItem and repoItem["use_submodules"] == False) else True
    local_exist = utils.Git.is_repository(external_path)
    
    if local_exist:
        cprint(repo_str + "Git reset...", 'yellow', attrs=['reverse', 'blink'])
        utils.Git.reset(external_path, printout=is_debug)
        cprint(repo_str + "Git pull...", 'yellow', attrs=['reverse', 'blink'])
        utils.Git.pull(external_path, printout=is_debug)
        cprint(repo_str + "Git pull ended", 'yellow', attrs=['reverse', 'blink'])
    else:
        if os.path.exists(external_path):
            cprint(repo_str + "Delete trash directory...", 'yellow', attrs=['reverse', 'blink'])
            utils.delete_folder(external_path)
        cprint(repo_str + "Git clone to " + ("'" + repoItem["branch"] + "'") if utils.string_not_empty(repoItem["branch"]) else ("default branch") + "...", 'yellow', attrs=['reverse', 'blink'])
        utils.Git.clone(external_path, repoItem["url"], branch=repoItem["branch"], printout=is_debug)
        cprint(repo_str + "Git clone ended", 'yellow', attrs=['reverse', 'blink'])

    if local_exist or utils.string_not_empty(revision):
        cprint(repo_str + "Git checkout to '" + repoItem["branch"] + (":" + revision if utils.string_not_empty(revision) else "") + "'...", 'yellow', attrs=['reverse', 'blink'])
        if revision.lower() == "latest":
            utils.Git.checkout(external_path, repoItem["branch"], printout=is_debug)
            SaveRevisionInfo(repoItem["project"])
        elif revision.lower() == "current":
            SaveRevisionInfo(repoItem["project"])
            utils.Git.checkout(external_path, repoItem["branch"], utils.Git.get_revision(external_path), printout=is_debug)
        else:
            utils.Git.checkout(external_path, repoItem["branch"], revision, printout=is_debug)
        cprint(repo_str + "Git checkout ended", 'yellow', attrs=['reverse', 'blink'])

    if use_submodules:
        cprint(repo_str + "Git submodules update...", 'yellow', attrs=['reverse', 'blink'])
        utils.Git.update_submodules(external_path, printout=is_debug)
        cprint(repo_str + "Git submodules update ended", 'yellow', attrs=['reverse', 'blink'])


def Reset():
    cprint("Reset project...", 'blue', attrs=['reverse', 'blink'])
    utils.call(['git', 'reset', '--hard'], base_dir)
    cprint("Reset project done\n", 'blue', attrs=['reverse', 'blink'])


def Checkout():
    cprint("Update Externals", 'cyan', attrs=['reverse', 'blink'])
    repo_list = project_info["dependencies"]
    if is_deprecated:
        for x in project_info["deprecated"]:
            repo_list.append(x)
    if is_debug:
        for repo_item in repo_list:
            UpdateExternal(repo_item)
    else:
        with ThreadPoolExecutor(max_workers=4) as executor:
            for repo_item in repo_list:
                executor.submit(UpdateExternal, repo_item)
    cprint("Update Externals End\n", 'cyan', attrs=['reverse', 'blink'])

def ApplyPatch():
    cprint("Apply patches...", 'green', attrs=['reverse', 'blink'])
    repo_list = project_info["dependencies"]
    for repo_item in repo_list:
        patchpath = os.path.abspath(patch_dir + "/" + repo_item["project"] + ".patch")
        filepath  = os.path.abspath(patch_dir + "/" + repo_item["project"])
        libpath   = os.path.abspath(base_dir + "/" + repo_item["path"])
        if os.path.exists(patchpath) and os.path.exists(libpath):
            cprint("Found " + repo_item["project"] + ".patch and try to apply it...", 'yellow', attrs=['reverse', 'blink'])
            utils.Git.apply_patch(lib_path=libpath, patch_path=patchpath)
            cprint("Patch applied to '" + repo_item["project"] + "'!\n", 'yellow', attrs=['reverse', 'blink'])

        if os.path.exists(filepath) and os.path.exists(libpath):
            cprint("Found " + repo_item["project"] + " folder and try to copy it...", 'yellow', attrs=['reverse', 'blink'])
            utils.copy_folder(filepath, libpath)
            cprint("Files copied to '" + repo_item["project"] + "'!\n", 'yellow', attrs=['reverse', 'blink'])
    cprint("Patches applied!\n", 'green', attrs=['reverse', 'blink'])


def CreatePatch():
    repo_list = project_info["dependencies"]
    for repo_item in repo_list:
        if repo_item["project"].lower() == libname_patch.lower():
            libpath   = os.path.abspath(base_dir + "/" + repo_item["path"])
            patchpath = os.path.abspath(patch_dir + "/" + repo_item["project"] + ".patch")
            if not os.path.exists(patch_dir):
                os.makedirs(patch_dir)
            utils.Git.create_patch(lib_path=libpath, patch_path=patchpath)
            cprint("Patch created '" + patchpath + "'!", 'green', attrs=['reverse', 'blink'])


def Premake():
    if not os.path.exists(premake_path):
        DownloadPremake()
    cprint("Generate projects...", 'magenta', attrs=['reverse', 'blink'])
    utils.call([premake_path, "Qt", "--target=linux", "--to=Qt-linux"], prj_dir)
    utils.call([premake_path, "Qt", "--target=windows", "--to=Qt-windows"], prj_dir)
    cprint("Projects generated !", 'magenta', attrs=['reverse', 'blink'])


def InstallAQT():
    global qt_dir
    qt_platform = "linux" if "linux" in sys.platform else "windows"
    qt_dir = os.path.abspath("{}/Qt_{}/".format(base_dir, qt_platform))
    qt_list = os.listdir(qt_dir) if os.path.exists(qt_dir) else {}
    if qt_version not in qt_list:
        utils.call([sys.executable, "-m", "pip", "install", "aqtinstall"], base_dir)
        if "linux" in sys.platform:
            utils.call([sys.executable, "-m", "aqt", "install-qt", qt_platform, "desktop", qt_version, compiler_name + "_" + compiler_arch, "--archives", "qtwayland", "qtbase", "icu", "--outputdir", qt_dir], base_dir)    
        else:
            utils.call([sys.executable, "-m", "aqt", "install-qt", qt_platform, "desktop", qt_version, platform_var + compiler_arch + "_" + compiler_name, "--archives", "qtbase", "MinGW", "--outputdir", qt_dir], base_dir)
    
    qt_list = os.listdir(qt_dir + "/Tools/") if os.path.exists(qt_dir + "/Tools/") else {}
    nodotversion = compiler_version
    if (compiler_name + nodotversion.replace(".","") + "_" + compiler_arch) not in qt_list and "linux" not in sys.platform:
        utils.call([sys.executable, "-m", "aqt", "install-tool", qt_platform, "desktop", aqt_compiler, "--outputdir", qt_dir], base_dir)
    if is_aqtcreator and "QtCreator" not in qt_list:
        qtcreator_dir = os.path.abspath(qt_dir + "/Tools/QtCreator/")
        utils.call([sys.executable, "-m", "aqt", "install-tool", qt_platform, "desktop", "tools_" + aqt_creator, "qt.tools." + aqt_creator, "--outputdir", qt_dir if "linux" in sys.platform else qtcreator_dir], base_dir)
        if "linux" not in sys.platform:
            utils.call([sys.executable, "-m", "aqt", "install-tool", qt_platform, "desktop", "tools_" + aqt_creator, "qt.tools.qtcreatorcdbext", "--outputdir", qt_dir], base_dir)


def SetupBuildEnvironment():
    nodotversion  = compiler_version
    compiler_dir  = qt_dir + "/Tools/" + compiler_name + nodotversion.replace(".","") + "_" + compiler_arch + "/bin/"
    split = [ x for x in os.environ["PATH"].split(os.pathsep) if "mingw" not in x ]
    os.environ["PATH"] = os.pathsep.join(split) + os.pathsep + os.pathsep.join([os.path.abspath(compiler_dir)])


def Build():
    cprint("Build '"+ prj_type + "' started...", 'yellow', attrs=['reverse', 'blink'])
    prj_path      = prj_dir + "/" + prj_type + "/" + prj_name + ".pro"
    build_cache   = prj_dir + "/" + prj_type + "_Release/"
    makefile_path = build_cache + "/Makefile"
    qmake_path    = qt_dir + "/" + qt_version + "/" + compiler_name + "_" + compiler_arch + "/bin/qmake" + exe_ext
    nodotversion  = compiler_version
    compiler_dir  = qt_dir + "/Tools/" + compiler_name + nodotversion.replace(".","") + "_" + compiler_arch + "/bin/"
    make_path     = ("make" if "linux" in sys.platform else (compiler_dir + "mingw32-make")) + exe_ext
    build_final   = build_dir + "/" + prj_type + "/bin/"
    SetupBuildEnvironment()
    
    cprint("Generate makefile from qmake...", 'yellow', attrs=['reverse', 'blink'])
    if not os.path.exists(build_cache):
        os.makedirs(build_cache)
    utils.call([qmake_path, prj_path, "-spec", "linux-g++" if "linux" in sys.platform else "win32-g++", "\"CONFIG+=qtquickcompiler\""], build_cache)

    cprint("Build qmake...", 'yellow', attrs=['reverse', 'blink'])
    utils.call([make_path, "-f", makefile_path, "qmake_all"], build_cache)

    cprint("Build app...", 'yellow', attrs=['reverse', 'blink'])
    utils.call([make_path, "-j8"], build_cache)

    build_exe = build_final + prj_name + exe_ext
    if os.path.exists(build_exe):
        Deploy()
    else:
        sys.exit("Build failed!")

    cprint("Build success!", 'yellow', attrs=['reverse', 'blink'])


def Deploy():
    cprint("Deploy app...", 'yellow', attrs=['reverse', 'blink'])
    build_final   = build_dir + "/" + prj_type + "/bin/"
    build_exe = build_final + prj_name + exe_ext
    if sys.platform == "win32" or sys.platform == "cygwin" or sys.platform == "msys":
        deploy_path   = qt_dir + "/" + qt_version + "/" + compiler_name + "_" + compiler_arch + "/bin/windeployqt" + exe_ext
        utils.call([deploy_path, build_exe, "--no-opengl-sw", "--no-translations", "--no-system-d3d-compiler"], build_final)
    else:
        print("Skip this for now...")


def Archive():
    filename = "{appname}-{version}-{subver}-qt{qtver}-{compiler}{compver}-{platform}"
    platform_str   = platform_var + compiler_arch
    version_str    = "v" + project_info["version"]
    subversion_str = project_info["status"]
    if is_nightly:
        version_str = "nightly"
        now = datetime.datetime.now()
        subversion_str = now.strftime('%Y%m%d%H%M%S')
    filename = filename.format(appname=prj_name, version=version_str, subver=subversion_str, qtver=qt_version, compiler=compiler_name, compver=compiler_version, platform=platform_str)
    input_dir = os.path.abspath(build_dir + "/" + prj_type + "/bin")
    build_zip = os.path.abspath(base_dir + "/" + filename + ".zip")

    list_dir = os.listdir(base_dir)
    for x in list_dir:
        if x.endswith(".zip"):
            os.remove(os.path.join(base_dir, x))
    utils.ArchiveZip(input_dir, build_zip, [".pdb", ".xml"])


def OpenCreator():
    cprint("Open the project to QtCreator...", 'yellow', attrs=['reverse', 'blink'])
    prj_type      = "Qt-linux" if "linux" in sys.platform else "Qt-windows"
    exe_ext       = "" if "linux" in sys.platform else ".exe"
    prj_path      = prj_dir + "/" + prj_type + "/" + prj_name + ".pro"
    CreatorPath   = qt_dir + "/Tools/QtCreator/bin/qtcreator" + exe_ext

    mingw_dir    = qt_dir + "/" + qt_version + "/" + compiler_name + "_" + compiler_arch + "/bin"
    os.environ["PATH"] += os.pathsep + os.pathsep.join([mingw_dir])

    nodotversion  = compiler_version
    mingw_dir  = qt_dir + "/Tools/" + compiler_name + nodotversion.replace(".","") + "_" + compiler_arch + "/bin"
    os.environ["PATH"] += os.pathsep + os.pathsep.join([mingw_dir])

    SetupBuildEnvironment()
    utils.call([CreatorPath, prj_path], base_dir)


def Execute():
    LoadInfo()
    if len(release_tag) > 0:
        UpdateInfoFromTag(release_tag)
    if is_reset is True:
        Reset()
    if is_checkout is True:
        Checkout()
    if is_apply_patch is True:
        ApplyPatch()
    if is_create_patch is True:
        CreatePatch()
    if is_premake is True:
        Premake()
    if is_aqtinstaller is True or is_aqtcreator is True:
        InstallAQT()
    if is_build is True:
        Build()
    if is_archive is True:
        Archive()
    if is_aqtcreator is True:
        OpenCreator()


if __name__ == "__main__":
    colorama.init()
    for arg in sys.argv:
        if "--debug" in arg:
            is_debug = True
        if "--checkout" in arg:
            is_checkout = True
        if "--deprecated" in arg:
            is_deprecated = True
        if "--reset" in arg:
            is_reset = True
        if "--patch" in arg:
            is_apply_patch = True
        if "--qt" in arg:
            qt_split = arg.split('=')
            if len(qt_split) > 1:
                qt_version = qt_split[1].strip()
        if "--aqt" in arg:
            is_aqtinstaller = True
            aqt_split = arg.split('=')
            if len(aqt_split) > 1:
                qt_version = aqt_split[1].strip()
                if qt_version.endswith("ide"):
                    is_aqtcreator = True
                    qt_version = qt_version.removesuffix("ide")
        if "--build" in arg:
            is_build = True
            platform_split = arg.split('=')
            if len(platform_split) > 1:
                platform_var = platform_split[1].strip()
        if "--premake" in arg:
            is_premake = True
            premake_split = arg.split('=')
            if len(premake_split) > 1:
                premake_variable = premake_split[1].strip()
        if "--create-patch" in arg:
            libname_split = arg.split('=')
            if len(libname_split) > 1:
                libname_patch = libname_split[1].strip()
                is_create_patch = True
        if "--archive" in arg:
            is_archive = True
            archive_split = arg.split('=')
            if len(archive_split) > 1:
                is_nightly = False if "release" in archive_split[1].strip() else True
        if "--tag" in arg:
            tag_split = arg.split('=')
            if len(tag_split) > 1:
                release_tag = tag_split[1].strip()
    Execute()
