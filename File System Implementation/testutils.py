from functools import wraps
from os import system
from unittest import TestCase
import hw3test as hw3


def fsck(method):
    @wraps(method)
    def wrapper_method(*args):
        method(*args)
        self = args[0]

        TestBase.run_fsck(self, "test.img")

    return wrapper_method


def blank_image(size):
    def decorator(method):
        @wraps(method)
        def wrapper_method(*args):
            self = args[0]

            system("rm test.img")
            v = system("./mkfs-hw3 -size " + str(size) + " test.img")
            self.assertEqual(v, 0)

            hw3.init("test.img")

            method(*args)

        return wrapper_method

    return decorator


class TestBase(TestCase):

    @staticmethod
    def read_img():
        system("./read-img test.img")

    def run_fsck(self, image="test.img"):
        v = system('python3 fsck.hw3 ' + image + ' > /dev/null')

        if v != 0:
            print('ERROR: fsck failed:')
            system('python3 fsck.hw3 test.img')
            self.assertEqual(v, 0, "fsck failed")

    def setUp(self):
        system("./mktest -extra test.img")
        hw3.init("test.img")

    def tearDown(self):
        system("rm test.img")
