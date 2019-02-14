#!/usr/bin/python3
import unittest
import hw3test as hw3
from testutils import *

class tests(TestBase):
    # def test_1_succeeds(self):
    #    print 'Test 1 - always succeeds'
    #    self.assertTrue(True)

    # def test_2_fails(self):
    #	print 'test 2 - always fails'
    #	self.assertTrue(False)

    @fsck
    def test_1_statfs(self):
        # print('Test 1 - statfs:')
        v, sfs = hw3.statfs()
        self.assertEqual(v, 0)
        self.assertEqual(sfs.f_bsize, 1024)  # any fsx600 file system
        self.assertEqual(sfs.f_blocks, 1024)  # only if image size = 1MB
        self.assertEqual(sfs.f_bfree, 720)  # for un-modified mktest output (with -extra)
        self.assertEqual(sfs.f_namemax, 27)  # any fsx600 file system

    @fsck
    def test_2_read(self):
        # print('Test 2 - getattr():')
        table = [('/dir1', 0o040755, 1024, 0x50000190),
                 ('/file.A', 0o100777, 1000, 0x500000C8),
                 ('/file.7', 0o100777, 6644, 0x5000012C)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(sb.st_mode, mode)
            self.assertEqual(sb.st_size, size)
            self.assertEqual(sb.st_ctime, ctime)

    @fsck
    def test_3_read_translate_case1(self):
        # print('Test 3 - test translate with ENOENT: - "/not-a-file"')
        table = [('/dir2', 0o040755, 1024, 0x50000191)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(v, - hw3.ENOENT)
            self.assertNotEqual(sb.st_mode, mode)

    @fsck
    def test_4_read_translate_case2(self):
        # print('Test 4 - test translate with ENOENT: - "dir1/not-a-file"')
        table = [('/dir1/file.8', 0o040755, 1024, 0x50000191)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(v, - hw3.ENOENT)
            self.assertNotEqual(sb.st_mode, mode)

    @fsck
    def test_5_read_translate_case3(self):
        # print('Test 5 - test translate with ENOENT: - "/not-a-dir/file.0"')
        table = [('/dir8/file.0', 0o040755, 1024, 0x50000191)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(v, - hw3.ENOENT)
            self.assertNotEqual(sb.st_mode, mode)

    @fsck
    def test_6_read_translate_case4(self):
        # print('Test 6 - test translate with ENOTDIR:- "/file.A/file.0"')
        table = [('/file.7/file.0', 0o040755, 1024, 0x50000191)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(v, - hw3.ENOTDIR)
            self.assertNotEqual(sb.st_mode, mode)

    @fsck
    def test_7_read(self):
        # print('Test 7 - big data read')
        data = bytes('K' * 276177, 'utf-8')
        v, result = hw3.read("/dir1/file.270", len(data) + 100, 0)  # offset=0
        self.assertEqual(v, len(data))
        self.assertEqual(result, data)

    @fsck
    def test_8_read(self):
        # print('Test 8 - Multiple smaller reads')
        data = 'K' * 10
        table = [17, 100, 1000, 1024, 1970, 3000]
        for data_length in table:
            data += 'K' * data_length
            v, result = hw3.read("/dir1/file.270", len(data), data_length)
            self.assertEqual(v, (len(data)))
            self.assertEqual(result, bytes(data, 'utf-8'))

    @fsck
    def test_9_readdir(self):
        # print('Test 9 - Test for readdir()')
        v, dirents = hw3.readdir("/")
        names = set([d.name.decode('utf-8') for d in dirents])
        self.assertEqual(names, set(('file.A', 'dir1', 'file.7', 'file.10')))
        table = [('/dir1', 0o040755, 1024, 0x50000190),
                 ('/file.A', 0o100777, 1000, 0x500000C8),
                 ('/file.7', 0o100777, 6644, 0x5000012C),
                 ('/file.10', 0o100777, 10234, 0x5000012C)]
        for path, mode, size, ctime in table:
            v, sb = hw3.getattr(path)
            self.assertEqual(sb.st_mode, mode)
            self.assertEqual(sb.st_size, size)
            self.assertEqual(sb.st_ctime, ctime)

    @fsck
    def test_dir_read(self):
        v, data = hw3.read("/dir1", 0, 0)
        self.assertEqual(v, -hw3.EISDIR)


unittest.main()
