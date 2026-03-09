from typing import Optional
import unittest
import os
import argparse
import sys
from pathlib import Path


def get_tests(test_collection):
    for test in test_collection:
        if isinstance(test, unittest.TestCase):
            yield test
            continue

        for sub_test in get_tests(test):
            yield sub_test


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--list",
        action="store_true",
        help="Discover all available test and print the ID of each of them on a separate line",
    )

    parser.add_argument(
        "--syspath",
        action="append",
        help="Add this path to sys.path before loading tests",
    )

    parser.add_argument("root", help="The root directory where to start looking for tests")

    parser.add_argument("test", nargs="?", help="The ID of the test to run")

    args = parser.parse_args()

    if args.syspath:
        for syspath in args.syspath:
            sys.path.append(syspath)

    test_suite = unittest.defaultTestLoader.discover(args.root)

    if args.list:
        for test in get_tests(test_suite):
            print(test.id())

        return

    if args.test:
        found_test: Optional[unittest.TestCase] = None
        for test in get_tests(test_suite):
            if test.id() == args.test:
                found_test = test
                break

        if found_test is None:
            print(f"No test named {args.test}", file=sys.stderr)
            sys.exit(1)

        test_suite = unittest.TestSuite([found_test])

    runner = unittest.TextTestRunner(verbosity=10)
    result = runner.run(test_suite)

    if not result.wasSuccessful():
        sys.exit(1)

if __name__ == "__main__":
    main()
