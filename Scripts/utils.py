from __future__ import print_function
import os
import sys
import subprocess
import shutil
import stat

def shellout(command, work_dir, **kwargs):
    process = subprocess.Popen(command, cwd=work_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdoutdata, stderrdata = process.communicate()

    print_output = kwargs.get("printout")
    if print_output == True:
        if stdoutdata is not None:
            print(stdoutdata.decode("utf8"))

        if stderrdata is not None:
            print(stderrdata.decode("utf8"))

    return process.returncode


def call(command, work_dir=None, **kwargs):
    if work_dir != None:
        os.chdir(work_dir)

    try:
        subprocess.check_call(command, cwd=work_dir, stderr=subprocess.STDOUT, **kwargs)
    except subprocess.CalledProcessError:
        sys.exit("Operation failed!")


def call_output(command, work_dir='', **kwargs):
    if work_dir == '':
        work_dir = None

    status = 0
    output = ''
    kwargs["universal_newlines"] = True
    kwargs['stdin'] = subprocess.PIPE
    process = subprocess.Popen(command, cwd=work_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    try:
        output, stderrdata = process.communicate()
    except subprocess.CalledProcessError as e:
        status = e.returncode
        output = e.output
    return [status, output]


def string_not_empty(s):
    return bool(s and s.strip())


def delete_folder(folder):
    def delete_folder_error_handler(*args):
        _, path, _ = args
        os.chmod(path, stat.S_IWRITE)
        os.remove(path)
    shutil.rmtree(folder, onerror=delete_folder_error_handler)

def delete_files(fileList):
    for f in fileList:
        if os.path.exists(f):
            if os.path.isdir(f) and f != ".":
                #context.Print('Deleting: {}'.format(f))
                try:
                    shutil.rmtree(f)
                except:
                    #context.Print("Unexpected error: {}".format(sys.exc_info()[0]))
                    return False

            elif os.path.isfile(f):
                #context.Print('Deleting: {}'.format(f))
                os.remove(f)
    return True

def copy_folder(source, destination, force=True):
    source = os.path.abspath(source)
    destination = os.path.abspath(destination)
    execute_copy = not os.path.exists(destination) or force

    if not execute_copy:
        return

    if os.path.isfile(source):
        dest_dir = os.path.dirname(destination)
        if not os.path.isdir(dest_dir):
            os.makedirs(dest_dir)
        shutil.copyfile(source, destination)
        shutil.copymode(source, destination)
    elif os.path.isdir(source):
        if not os.path.isdir(destination):
            os.makedirs(destination)
        for f in os.listdir(source):
            copy_folder(os.path.join(source, f), os.path.join(destination, f))


#Git stuff...
class Git:
    def __init__(self):
        pass

    @staticmethod
    def is_repository(path):
        if not os.path.exists(path):
            return False
        if not os.path.exists(os.path.join(path, ".git")):
            return False
        if len(os.listdir(path)) <= 1:
            return False
        return shellout(["git", "rev-parse", "--git-dir"], path) == 0

    @staticmethod
    def clone(path, url, **kwargs):
        if not os.path.exists(path):
            os.makedirs(path)
        branch = kwargs.get("branch")
        if string_not_empty(branch):
            shellout(["git", "clone", "--branch", branch, url, path], path, **kwargs)
        else:
            shellout(["git", "clone", url, path], path, **kwargs)

    @staticmethod
    def pull(path, **kwargs):
        shellout(["git", "pull", "--all"], path, **kwargs)
        shellout(["git", "remote", "update", "origin", "--prune"], path, **kwargs)

    @staticmethod
    def checkout(path, location, revision="", **kwargs):
        if len(revision) > 0:
            shellout(["git", "checkout", "-B", location, revision], path, **kwargs)
        else:
            shellout(["git", "checkout", "-f", location], path, **kwargs)

    @staticmethod
    def update_submodules(path, **kwargs):
        shellout(["git", "submodule", "update", "--init", "--recursive"], path, **kwargs)

    @staticmethod
    def reset(path, **kwargs):
        shellout(['git', 'reset', '--hard'], path, **kwargs)

    @staticmethod
    def create_patch(lib_path, patch_path, **kwargs):
        call(["git", "diff", "--ignore-submodules", "--output=" + patch_path], lib_path)

    @staticmethod
    def apply_patch(lib_path, patch_path, **kwargs):
        shellout(['git', 'reset', '--hard'], lib_path)
        call(["git", "apply", "--reject", "--whitespace=fix", patch_path], lib_path, **kwargs)

    @staticmethod
    def get_revision(lib_path, **kwargs):
        status, output = call_output(["git", "rev-parse", "HEAD"], lib_path, **kwargs)
        if status == 0:
            return output.decode("utf8").strip()
        else:
            sys.exit("Git error: " + output)