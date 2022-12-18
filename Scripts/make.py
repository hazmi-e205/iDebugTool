import sys
import os
import json
from concurrent.futures import ThreadPoolExecutor
from urllib.request import urlopen
import tarfile, zipfile
from io import BytesIO

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
premake_path    = os.path.abspath(script_dir + "/premake5")
if sys.platform == "win32" or sys.platform == "cygwin" or sys.platform == "msys":
    premake_path = premake_path + ".exe"

#compiler
qt_dir           = "C:/Qt"
qt_version       = "6.4.1"
compiler_name    = "mingw"
compiler_version = "1120"
compiler_arch    = "64"
platform_var     = "win"
prj_name         = "iDebugTool"

#flags
is_checkout     = False
is_reset        = False
is_premake      = False
is_apply_patch  = False
is_create_patch = False
is_build        = False
is_debug        = False


def DownloadPremake():
    request_releases = urlopen("https://api.github.com/repos/Premake/premake-core/releases").read()
    premake_releases = json.loads(request_releases)
    premake_latest   = premake_releases[0]["assets"]
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


def Build():
    prj_type      = "Qt-windows"
    exe_ext       = ".exe"
    if "lin" in platform_var:
        prj_type  = "Qt-linux"
        exe_ext   = ""
    
    cprint("Build '"+ prj_type + "' started...", 'yellow', attrs=['reverse', 'blink'])
    prj_path      = prj_dir + "/" + prj_type + "/" + prj_name + ".pro"
    build_cache   = prj_dir + "/" + prj_type + "_Release/"
    makefile_path = build_cache + "/Makefile"
    qmake_path    = qt_dir + "/" + qt_version + "/" + compiler_name + "_" + compiler_arch + "/bin/qmake" + exe_ext
    deploy_path   = qt_dir + "/" + qt_version + "/" + compiler_name + "_" + compiler_arch + "/bin/windeployqt" + exe_ext
    compiler_dir  = qt_dir + "/Tools/" + compiler_name + compiler_version + "_" + compiler_arch + "/bin/"
    make_path     = compiler_dir + "mingw32-make" + exe_ext
    build_final   = build_dir + "/" + prj_type + "/bin/"
    
    cprint("Generate makefile from qmake...", 'yellow', attrs=['reverse', 'blink'])
    os.environ["PATH"] += os.pathsep + os.pathsep.join([compiler_dir])
    if not os.path.exists(build_cache):
        os.makedirs(build_cache)
    utils.call([qmake_path, prj_path, "-spec", "win32-g++", "\"CONFIG+=qtquickcompiler\""], build_cache)

    cprint("Build qmake...", 'yellow', attrs=['reverse', 'blink'])
    utils.call([make_path, "-f", makefile_path, "qmake_all"], build_cache)

    cprint("Build app...", 'yellow', attrs=['reverse', 'blink'])
    utils.call([make_path, "-j8"], build_cache)

    cprint("Deploy app...", 'yellow', attrs=['reverse', 'blink'])
    build_exe = build_final + prj_name + exe_ext
    if os.path.exists(build_exe):
        utils.call([deploy_path, build_exe, "--no-opengl-sw", "--no-translations", "--no-system-d3d-compiler"], build_final)
    else:
        sys.exit("Build failed!")

    cprint("Build success!", 'yellow', attrs=['reverse', 'blink'])


def Execute():
    LoadInfo()
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
    if is_build is True:
        Build()


if __name__ == "__main__":
    colorama.init()
    for arg in sys.argv:
        if "--debug" in arg:
            is_debug = True
        if "--checkout" in arg:
            is_checkout = True
        if "--reset" in arg:
            is_reset = True
        if "--patch" in arg:
            is_apply_patch = True
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
    Execute()