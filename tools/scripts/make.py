import sys
import os
import json
from concurrent.futures import ThreadPoolExecutor

import utils
import colorama
from termcolor import cprint


script_dir      = os.path.dirname(os.path.abspath(__file__))
base_dir        = os.path.abspath(script_dir + "/../../")
prj_dir         = os.path.abspath(base_dir + "/prj/")
build_dir       = os.path.abspath(base_dir + "/build/")
external_dir    = os.path.abspath(base_dir + "/Externals/")
patch_dir       = os.path.abspath(base_dir + "/Externals/_Patches/")
info_path       = os.path.abspath(base_dir + "/info.json")

#flags
is_checkout     = False
is_reset        = False
is_premake      = False
is_apply_patch  = False
is_create_patch = False
is_build        = False
is_debug        = False


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
    cprint("Please do something...", 'yellow', attrs=['reverse', 'blink'])


def CreatePatch():
    cprint("Please do something...", 'yellow', attrs=['reverse', 'blink'])


def Premake():
    cprint("Please do something...", 'yellow', attrs=['reverse', 'blink'])


def Build():
    cprint("Please do something...", 'yellow', attrs=['reverse', 'blink'])


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