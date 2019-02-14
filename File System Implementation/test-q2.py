#!/usr/bin/python3
import unittest
from time import time

from testutils import *
import hw3test as hw3


class tests(TestBase):

    @fsck
    def test_1_mkdir(self):
        # print('Test 1 - Mkdir on good path')
        attrs = [("/dir2", 0),
                 ("/dir2/hello", 0o777),
                 ("/dir2/hello/world", 0o744)]

        for path, perm in attrs:
            self.assertEqual(0, hw3.mkdir(path, perm))

        for path, perm in attrs:
            v, attr = hw3.getattr(path)
            self.assertEqual(v, 0)
            self.assertEqual(attr.st_mode, perm | hw3.S_IFDIR)

            v, entries = hw3.readdir(path)
            numents = 0 if perm == 0o744 else 1

            self.assertEqual(v, numents)
            self.assertEqual(numents, len(entries), path)

    @fsck
    def test_mkdir_create_inval(self):
        self.assertEqual(hw3.mkdir("/dir2", 0), 0)
        self.assertEqual(hw3.mkdir("/dir2/hello", 0), 0)
        self.assertEqual(hw3.create("/dir2/file", 0), 0)

        errs = [("/", -hw3.EEXIST),
                ("/dir2/hello", -hw3.EEXIST),
                ("/dir2/file", -hw3.EEXIST),
                ("/dir2/bogus/new", -hw3.ENOENT),
                ("/dir2/file/sub", -hw3.ENOTDIR)]

        for path, err in errs:
            self.assertEqual(hw3.mkdir(path, 0), err, path)
            self.assertEqual(hw3.create(path, 0), err, path)

    @fsck
    @blank_image(1024 * 1024 * 4)
    def test_2_mkdir_deep(self):
        num = 250  # Could go deeper, but it takes O(n^2) (if not more) time

        for i in range(1, num + 1):
            pathname = "/a" * i

            v = hw3.mkdir(pathname, 0)
            self.assertEqual(v, 0)

        v = hw3.create("/a" * num + "/deep.txt", 0)
        self.assertEqual(v, 0)

        v = hw3.write("/a" * num + "/deep.txt", b"This is deep stuff", 0)
        self.assertEqual(v, 18)

        v, b = hw3.read("/a" * num + "/deep.txt", 1000, 0)
        self.assertEqual(v, 18)
        self.assertEqual(b, b"This is deep stuff")

    @fsck
    @blank_image(1024 * 1024 * 4)
    def test_3_mkdir_wide(self):
        num = 500

        hw3.mkdir("/hello", 0)

        for i in range(num):
            v = hw3.mkdir("/hello/" + str(i), i)
            self.assertEqual(v, 0)

        n, entries = hw3.readdir("/hello")
        self.assertEqual(n, num)

    @fsck
    def test_4_write(self):
        # print('Test 4 - Write test - write a big block')
        data = b'B' * 6644
        hw3.write("/file.7", data, 0)  # offset=0
        v, result = hw3.read("/file.7", len(data), 0)
        self.assertEqual(v, (len(data)))
        self.assertEqual(result, data),

    @fsck
    def test_5_write(self):
        # print('Test 5 - Write test - write multiple smaller blocks')
        data = 'M' * 10
        table = [100, 1000, 1024, 3000]
        b_count = 10
        for data_length in table:
            data += 'M' * data_length
            b_count += data_length
            hw3.write("/file.7", bytes(data, 'utf-8'), 0)
        v, result = hw3.read("/file.7", 10000, 0)
        self.assertEqual(v, 6644)

        num_4 = 6644 - len(data)
        expected = data + ("4" * num_4)
        self.assertEqual(result, bytes(expected, 'utf-8'))

    @fsck
    def test_6_truncate(self):
        # print('Test 6 - Truncate a file')
        hw3.truncate('/dir1/file.2', 0)
        v, dirents = hw3.readdir("/dir1/file.2")
        self.assertEqual(v, - hw3.ENOTDIR)

    @fsck
    def test_7_unlink(self):
        # print('Test 7 - Unlink a file')
        v = hw3.unlink('/file.A')
        self.assertEqual(v, 0)
        v, dirents = hw3.readdir("/file.A")
        self.assertEqual(v, - hw3.ENOENT)

    @fsck
    def test_unlink_big(self):
        v = hw3.unlink('/dir1/file.270')
        self.assertEqual(v, 0)
        v, data = hw3.read("/dir1/file.270", 1000, 0)
        self.assertEqual(v, -hw3.ENOENT)

    @fsck
    @blank_image(1024 * 1024 * 32)
    def test_create_write_concurrent(self):

        self.assertEqual(hw3.create("/", 0), -hw3.EEXIST)

        big_file = 512 * 1024

        self.assertEqual(hw3.mkdir("/dir2", 0), 0)
        self.assertEqual(hw3.create("/dir2/.file", 0), 0)
        self.assertEqual(hw3.write("/dir2/.file", bytes("Hello, world", 'utf-8'), 0), 12)
        self.assertEqual(hw3.create("/dir2/file.2", 0), 0)
        self.assertEqual(hw3.write("/dir2/file.2", b'M' * big_file, 0), big_file)

        self.assertEqual(hw3.create("/dir2/.file", 0), -hw3.EEXIST)

        self.run_fsck()

        v, data = hw3.read("/dir2/.file", 1000, 7)
        self.assertEqual(v, 5)
        self.assertEqual(data.decode('utf-8'), "world")

        self.run_fsck()

        v, data = hw3.read("/dir2/file.2", big_file, 0)
        self.assertEqual(v, big_file)
        self.assertEqual(data, b'M' * big_file)

        self.run_fsck()

        self.assertEqual(hw3.unlink("/dir2/file.2"), 0)
        self.run_fsck()

        self.assertEqual(hw3.unlink("/dir2/.file"), 0)
        self.run_fsck()

        v, data = hw3.readdir("/dir2")
        self.assertEqual(v, 0)
        self.assertEqual(data, [])

        self.run_fsck()

        self.assertEqual(hw3.rmdir("/dir2"), 0)

    @fsck
    @blank_image(1024 * 1024 * 4)
    def test_create(self):
        num = 500

        for i in range(num):
            mode = i % 0o1000
            v = hw3.create("/" + str(i), mode)
            self.assertEqual(v, 0)
            v, attr = hw3.getattr("/" + str(i))
            self.assertEqual(v, 0)
            self.assertEqual(attr.st_size, 0)
            self.assertEqual(attr.st_mode, mode | hw3.S_IFREG)

    @fsck
    @blank_image(1024 * 1024)
    def test_long_name(self):
        files = {"Hello, world",
                 "12345678901234567890",
                 "123456789012345678901234567",
                 "1" * 28,
                 "2" * 35}

        endfiles = {f[0:27] for f in files}

        for file in files:
            v = hw3.create("/" + file, 0)
            self.assertEqual(v, 0)
            v, attr = hw3.getattr(file)
            self.assertEqual(v, 0)

        v, rd = hw3.readdir("/")
        self.assertEqual({s.name.decode("utf-8") for s in rd}, endfiles)

        for efile in endfiles:
            v, attr = hw3.getattr("/" + efile)
            self.assertEqual(v, 0)

        self.assertEqual(hw3.create("2" * 44, 0), -hw3.EEXIST)

    @fsck
    @blank_image(1024 * 1100)
    def test_stress_write_unlink(self):

        v, res = hw3.statfs()
        self.assertEqual(v, 0)
        blocks_free = res.f_bavail

        sizes = [12, 577, 1011, 1024, 1025, 2001, 8099, 37000, 289150, 1024 * 1024]
        writes = [17, 100, 1000, 1024, 1970]

        cases = [(size, write, "/%s-%s" % (size, write)) for size in sizes for write in writes]

        bigdata = b'123456789' * 33333333

        for size, write, name in cases:
            towrite = bigdata[0:size]

            v = hw3.create(name, 0)
            self.assertEqual(v, 0)

            idx = 0
            while idx < size:
                maxwrite = min(size, idx + write)
                data = towrite[idx:maxwrite]

                v = hw3.write(name, data, idx)
                self.assertEqual(v, len(data))

                idx += write

            v, data = hw3.read(name, size + 10000, 0)
            self.assertEqual(v, size)
            self.assertEqual(len(data), size)

            self.assertEqual(data, towrite, name)

            # Truncate before the unlink roughly half of the time
            if size % 2 == 0:
                v = hw3.truncate(name, 0)
                self.assertEqual(v, 0)

                v, attr = hw3.getattr(name)
                self.assertEqual(v, 0)
                self.assertEqual(attr.st_size, 0)

            v = hw3.unlink(name)
            self.assertEqual(v, 0)

            v, attr = hw3.getattr(name)
            self.assertEqual(v, -hw3.ENOENT)

            v, stat = hw3.statfs()
            self.assertEqual(v, 0)
            self.assertEqual(stat.f_bavail, blocks_free)

    @fsck
    @blank_image(1024 * 1100)
    def test_stress_write_truncate(self):

        v, res = hw3.statfs()
        self.assertEqual(v, 0)
        blocks_free = res.f_bfree

        sizes = [12, 577, 1011, 1024, 1025, 2001, 8099, 37000, 289150, 1024 * 1024]

        cases = [(size, "/%s" % (size)) for size in sizes]

        bigdata = b'123456789' * 33333333

        for size, name in cases:
            towrite = bigdata[0:size]

            v = hw3.create(name, 0)
            self.assertEqual(v, 0)

            v = hw3.write(name, towrite, 0)
            self.assertEqual(v, size)

            v, data = hw3.read(name, size + 10000, 0)
            self.assertEqual(v, size)
            self.assertEqual(len(data), size)

            self.assertEqual(data, towrite, name)

            # Truncate before the unlink roughly half of the time
            v = hw3.truncate(name, 0)
            self.assertEqual(v, 0)

            v, attr = hw3.getattr(name)
            self.assertEqual(v, 0)
            self.assertEqual(attr.st_size, 0)

            v = hw3.unlink(name)
            self.assertEqual(v, 0)

            v, attr = hw3.getattr(name)
            self.assertEqual(v, -hw3.ENOENT)

            v, stat = hw3.statfs()
            self.assertEqual(v, 0)
            self.assertEqual(stat.f_bfree, blocks_free)

    @fsck
    @blank_image(1024 * 100)
    def test_rmdir_valid(self):
        hw3.mkdir("/hello", 0)

        v, attr = hw3.getattr("/hello")
        self.assertEqual(v, 0)

        v = hw3.rmdir("/hello")
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/hello")
        self.assertEqual(v, -hw3.ENOENT)

        v, dirs = hw3.readdir("/hello")
        self.assertEqual(v, -hw3.ENOENT)

    @fsck
    def test_rmdir_inval(self):
        v = hw3.rmdir("/dir1")
        self.assertEqual(v, -hw3.ENOTEMPTY)

        v = hw3.rmdir("/dir2")
        self.assertEqual(v, -hw3.ENOENT)

        hw3.unlink("/dir1/file.2")
        hw3.unlink("/dir1/file.0")
        hw3.unlink("/dir1/file.270")

        v = hw3.unlink("/dir1")
        self.assertEqual(v, -hw3.EISDIR)

        v = hw3.rmdir("/dir1")
        self.assertEqual(v, 0)

    @fsck
    def test_rename_good(self):
        v = hw3.rename("/dir1", "/dir5")
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/dir5")
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/dir1")
        self.assertEqual(v, -hw3.ENOENT)

        v = hw3.rename("/dir5/file.270", "/bigfile")
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/bigfile")
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/dir5/file.270")
        self.assertEqual(v, -hw3.ENOENT)

    @fsck
    def test_rename_bad(self):
        v = hw3.rename("/dir2", "/dir3")
        self.assertEqual(v, -hw3.ENOENT)

        v = hw3.rename("/file.A", "/file.10")
        self.assertEqual(v, -hw3.EEXIST)

        v = hw3.rename("/file.A", "/dir1")
        self.assertEqual(v, -hw3.EEXIST)

        v = hw3.mkdir("/dir2", 0)
        self.assertEqual(v, 0)

        v = hw3.rename("/dir1", "/dir2")
        self.assertEqual(v, -hw3.EEXIST)

        v = hw3.rename("/dir1", "/file.A")
        self.assertEqual(v, -hw3.EEXIST)

    @fsck
    def test_chmod(self):
        v = hw3.chmod("/dir1", 0o723)
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/dir1")
        self.assertEqual(v, 0)
        self.assertEqual(attr.st_mode & ~hw3.S_IFMT, 0o723)

        v = hw3.chmod("/file.A", 0o611)
        self.assertEqual(v, 0)

        v, attr = hw3.getattr("/file.A")
        self.assertEqual(v, 0)
        self.assertEqual(attr.st_mode & ~hw3.S_IFMT, 0o611)

    @fsck
    def test_utime(self):

        names = ["/dir1", "/file.A"]

        now = int(time())

        times = [0, 1234534598, now]

        for name in names:

            for t in times:
                v = hw3.utime(name, 0, t)

                self.assertEqual(v, 0)

                v, attr = hw3.getattr(name)
                self.assertEqual(v, 0)

                theirtime = attr.st_mtime

                if t == 0:
                    self.assertAlmostEqual(theirtime, now, delta=2)
                else:
                    self.assertEqual(theirtime, t)

    @fsck
    def test_trunc_err(self):
        errs = [("/dir1", 0, hw3.EISDIR),
                ("/bogus", 0, hw3.ENOENT),
                ("/file.A", 1, hw3.EINVAL)]

        for name, len, err in errs:
            v = hw3.truncate(name, len)
            self.assertEqual(v, -err)

    @fsck
    def test_unlink_err(self):
        errs = [("/dir1", hw3.EISDIR),
                ("/bogus", hw3.ENOENT)]

        for name, err in errs:
            v = hw3.unlink(name)
            self.assertEqual(v, -err)

    @fsck
    def test_write_err(self):
        errs = [("/dir1", 0, hw3.EISDIR),
                ("/bogus", 0, hw3.ENOENT),
                ("/file.A", 1001, hw3.EINVAL)]

        for name, off, err in errs:
            v = hw3.write(name, "Hello", off)
            self.assertEqual(v, -err)

    @fsck
    @blank_image(1024 * 1024)
    def test_complex_rmdir(self):
        v, stat = hw3.statfs()
        self.assertEqual(v, 0)
        free = stat.f_bfree
        self.assertTrue(free >= 900)

        actions = [
            (hw3.mkdir("/test8", 0o777), 0),
            (hw3.mkdir("/test8/dir1", 0o777), 0),
            (hw3.mkdir("/test8/dir2", 0o777), 0),
            (hw3.mkdir("/test8/dir3", 0o777), 0),
            (hw3.mkdir("/test8/dir1/a", 0o777), 0),
            (hw3.mkdir("/test8/dir1/b", 0o777), 0),
            (hw3.create("/test8/dir2/c", 0o777), 0),
            (hw3.create("/test8/dir2/d", 0o777), 0),
            (hw3.rmdir("/test8/dir2/c"), -20),
            (hw3.rmdir("/test8/dir2"), -39),
            (hw3.rmdir("/test8/dir1"), -39),
            (hw3.unlink("/test8/dir3"), -21),
            (hw3.unlink("/test8/dir1/a"), -21),
            (hw3.rmdir("/test8/dir3"), 0),
            (hw3.rmdir("/test8/dir1/a"), 0),
            (hw3.rmdir("/test8/dir1/b"), 0),
            (hw3.unlink("/test8/dir2/c/foo"), -20),
            (hw3.unlink("/test8/dir1/q/foo"), -2),
            (hw3.rmdir("/test8/dir2/c/foo"), -20),
            (hw3.rmdir("/test8/dir1/q/foo"), -2),
            (hw3.unlink("/test8/dir1/q"), -2),
            (hw3.rmdir("/test8/dir1/q"), -2),
            (hw3.unlink("/test8/dir2/c"), 0),
            (hw3.unlink("/test8/dir2/d"), 0),
            (hw3.rmdir("/test8/dir2"), 0),
            (hw3.rmdir("/test8/dir1"), 0),
            (hw3.rmdir("/test8"), 0)
        ]

        actions = [(i, actions[i][0], actions[i][1]) for i in range(len(actions))]

        for i, result, expected in actions:
            self.assertEqual(result, expected, str(i) + ": " + hw3.strerr(result))

        v, stat = hw3.statfs()
        self.assertEqual(v, 0)
        self.assertEqual(free, stat.f_bfree)




unittest.main()
