from ctypes import *
#from enum import IntEnum
import os
import random

verbose = False

# 'struct stat' when compiled with _FILE_OFFSET_BITS=64
#
class stat(Structure):
    _fields_ = [("_pad1", c_char * 16),
                ("st_mode", c_uint),      # 16
                ("_pad2", c_char * 4),    # 20
                ("st_uid", c_uint),       # 24
                ("st_gid", c_uint),       # 28
                ("_pad3", c_char * 12),   # 32
                ("st_size", c_ulonglong), # 44
                ("_pad4", c_char * 20),   # 52
                ("st_mtime", c_uint),     # 72
                ("_pad5", c_char * 4),    # 76
                ("st_ctime", c_uint),     # 80
                ("_pad6", c_char * 12)]   # 84, len=96
    def __str__(self):
        return ('st_mode=%o st_uid=%d st_size=%d st_mtime=%d' %
                    (self.st_mode, self.st_uid, self.st_size, self.st_mtime))
    
# used by the readdir function - name plus stat buf
class dirent(Structure):
    _fields_ = [("name", c_char * 64), 
                ("_pad1", c_char * 16),
                ("st_mode", c_uint),
                ("_pad2", c_char * 4),
                ("st_uid", c_uint),
                ("st_gid", c_uint),
                ("_pad3", c_char * 12),
                ("st_size", c_ulonglong),
                ("_pad4", c_char * 20),
                ("st_mtime", c_uint),
                ("_pad5", c_char * 4),
                ("st_ctime", c_uint),
                ("_pad6", c_char * 12)]

class statvfs(Structure):
    _fields_ = [("f_bsize", c_uint),      # 0
                ("f_frsize", c_char * 4), # 4
                ("f_blocks", c_longlong), # 8
                ("f_bfree", c_longlong),  # 16
                ("f_bavail", c_longlong), # 24
                ("_pad", c_char * 36),    # 32
                ("f_namemax", c_uint),    # 68
                ("_pad2", c_char * 24)]   # 72 -> 96
    def __str__(self):
        return ('size=%d blocks=%d bfree=%d namemax=%d' %
                    (self.f_bsize, self.f_blocks, self.f_bfree, self.f_namemax))
    
class fuse_file_info(Structure):
    _fields_ = [("flags", c_int),         # 0
                ("_fh_old", c_ulong),     # 4
                ("_writepage", c_int),    # 8
                ("_flags", c_uint),       # 12
                ("fh", c_ulonglong),      # 16
                ("_lock_owner", c_ulonglong)]   # 24 -> 32

class fuse_context(Structure):
    _fields_ = [("_fuse", c_char * 4),
                ("uid", c_uint),
                ("gid", c_uint),
                ("pid", c_uint),
                ("_private", c_char * 4),
                ("_umask", c_char * 4)]

        
dir = os.getcwd()
hw3lib = CDLL(dir + "/libhw3.so")

assert hw3lib

ctx = hw3lib.ctx

null_fi = fuse_context()

# python3 - bytes(path) -> bytes(path, 'UTF-8')
#
def init(image):
    return hw3lib.hw3_init(bytes(image, 'UTF-8'))

def getattr(path):
    sb = stat()
    retval = hw3lib.hw3_getattr(bytes(path, 'UTF-8'), byref(sb))
    if verbose:
        print ('getattr(%s) = %d (%s)' %
                   (path, retval, strerr(retval) if retval < 0 else str(sb)))
    return retval, sb

dir_max = 100000
def readdir(path):
    des = (dirent * dir_max)()
    n = hw3lib.hw3_readdir(bytes(path, 'UTF-8'), c_int(dir_max), byref(des), byref(null_fi))
    if n > 0:
        if verbose:
            print('readdir(%s) = [%s]' % (path, ' '.join([de.name for de in des[0:n]])))
        return n, des[0:n]
    else:
        if verbose:
            print('readdir(%s) = %d (%s)' % (path, n, strerr(n)))
        return n, []
    
def create(path, mode):
    retval = hw3lib.hw3_create(bytes(path, 'utf-8'), c_int(mode), byref(null_fi))
    if verbose:
        print('create(%s,%o) = %d' % (path, mode, retval))
    return retval

def mkdir(path, mode):
    retval = hw3lib.hw3_mkdir(bytes(path, 'utf-8'), c_int(mode))
    if verbose:
        print('mkdir(%s,%o) = %d' % (path, mode, retval))
    return retval

def truncate(path, offset):
    retval = hw3lib.hw3_truncate(bytes(path, 'utf-8'), c_int(offset))
    if verbose:
        print('truncate(%s,%d) = %d' % (path, offset, retval))
    return retval

def unlink(path):
    retval = hw3lib.hw3_unlink(bytes(path, 'utf-8'))
    if verbose:
        print('unlink(%s) = %d' % (path, retval))
    return retval

def rmdir(path):
    retval = hw3lib.hw3_rmdir(bytes(path, 'utf-8'))
    if verbose:
        print('rmdir(%s) = %d' % (path, retval))
    return retval

def rename(path1, path2):
    retval = hw3lib.hw3_rename(bytes(path1, 'utf-8'), bytes(path2, 'utf-8'))
    if verbose:
        print('rename(%s,%s) = %d' % (path1, path2, retval))
    return retval

def chmod(path, mode):
    retval = hw3lib.hw3_chmod(bytes(path, 'utf-8'), c_int(mode))
    if verbose:
        print('rmdir(%s) = %d' % (path, retval))
    return retval

def utime(path, actime, modtime):
    retval = hw3lib.hw3_utime(bytes(path, "utf-8"), c_int(actime), c_int(modtime))
    if verbose:
        print('utime(%s,%d,%d) = %d' % (path, actime, modtime, retval))
    return retval

def read(path, len, offset):
    buf = (c_char * len)()
    val = hw3lib.hw3_read(bytes(path, "utf-8"), buf, c_int(len), c_int(offset), byref(null_fi))
    if verbose:
        print('read(%s,%d,%d) = %d' % (path, len, offset, val))
    if val < 0:
        return val,''
    return val, buf[0:val]

def write(path, data, offset):
    nbytes = len(data)
    val = hw3lib.hw3_write(bytes(path, "utf-8"), data, c_int(nbytes),
                               c_int(offset), byref(null_fi))
    if verbose:
        print('write(%s,%d,%d) = %d' % (path, nbytes, offset, val))
    return val

def statfs():
    st = statvfs()
    retval = hw3lib.hw3_statfs('/', byref(st))
    if verbose:
        print('statfs() = %d (%s)' % (retval, str(st)))
    return retval,st

# Constants

S_IFMT  = 0o0170000  # bit mask for the file type bit field
S_IFREG = 0o0100000  # regular file
S_IFDIR = 0o0040000  # directory

EPERM = 1            # Error codes
ENOENT = 2           # ...
EIO = 5
ENOMEM = 12
ENOTDIR = 20
EISDIR = 21
EINVAL = 22
ENOSPC = 28
EOPNOTSUPP = 95
EEXIST = 17
ENOTEMPTY = 39

errors = { EPERM : 'EPERM', ENOENT : 'ENOENT', EIO : 'EIO',
               ENOMEM : 'ENOMEM', ENOTDIR : 'ENOTDIR',
               EISDIR : 'EISDIR', EINVAL : 'EINVAL',
               ENOSPC : 'ENOSPC', EOPNOTSUPP : 'EOPNOTSUPP',
               EEXIST : 'EEXIST', ENOTEMPTY : 'ENOTEMPTY'}

def strerr(err):
    if err < 0:
        err = -err
    else:
        return "OK"
    if err in errors:
        return errors[err]
    return 'UNKNOWN (%d)' % err
