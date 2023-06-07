import os, sys, utils

extension = sys.argv[1]
dll_dir   = sys.argv[2]
dst_dir   = sys.argv[3]
list_dir  = os.listdir(dll_dir)
for x in list_dir:
    if x.endswith(extension) or x.endswith(extension):
        src_path = os.path.abspath(dll_dir + "/" + x)
        dst_path = os.path.abspath(dst_dir + "/" + x)
        print("Adding " + dst_path + "...")
        utils.copy_folder(src_path, dst_path)