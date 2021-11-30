#!/usr/bin/env python3
import unittest
import shutil
from os import path
from glob import glob

from text_converter import text_convert


class TestTextConverter(unittest.TestCase):
    def test(self):
        testdir = path.dirname(__file__)
        output = path.join(testdir, "output")
        data = path.join(testdir, "data")

        # copy data to output directory
        shutil.rmtree(output, ignore_errors=True)
        shutil.copytree(data, output)

        # convert all data in output directory
        for file in glob(path.join(output, "*")):
            self.assertTrue(text_convert(file), f"Failed converting {file}")

        # check the files!
        for file in glob(path.join(output, "*")):
            with open(file) as content:
                self.assertEqual(path.basename(file), content.read().strip())
