import argparse
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(__file__)

PACKAGE_DIRNAME = "package"
GTEST_PARALLEL_PATH = os.path.join(SCRIPT_DIR, "third_party", "gtest-parallel", "gtest-parallel")
CONFIGURE_SCRIPT_PATH = os.path.join(SCRIPT_DIR, "configure.py")

def configure(build_dir, target):
  if not os.path.isdir(build_dir):
    os.mkdir(build_dir)
  result = subprocess.run([sys.executable, CONFIGURE_SCRIPT_PATH, "--targets", target, "--enable-tests"], cwd=build_dir, shell=True)
  if result.returncode != 0:
    sys.exit(result.returncode)

def build(build_dir):
  result = subprocess.run(["ambuild"], cwd=build_dir, shell=True)
  if result.returncode != 0:
    sys.exit(result.returncode)

def run_tests(build_dir, target):
  test_binary = os.path.join(build_dir, PACKAGE_DIRNAME, target, "testrunner")
  if sys.platform == "win32":
    test_binary += ".exe"
  elif sys.platform == "linux":
    pass
  else:
    raise OSError(f"Unsupported platform: {sys.platform}")

  if not os.path.isfile(test_binary):
    print(f"Test binary not found: {test_binary}")
    sys.exit(1)
  gtest_parallel = os.path.abspath(GTEST_PARALLEL_PATH)
  result = subprocess.run([sys.executable, gtest_parallel, test_binary], shell=True)
  sys.exit(result.returncode)

def main():
  parser = argparse.ArgumentParser(description="Build and run the test suite.")
  parser.add_argument("--target", choices=["x86", "x86_64"], required=True,
                      help="Target architecture to build and test.")
  parser.add_argument("--build-dir", default="build",
                      help="Build directory.")
  parser.add_argument("--skip-build", action="store_true",
                      help="Skip build step.")
  parser.add_argument("--skip-configure", action="store_true",
                      help="Skip build configuration step.")
  args = parser.parse_args()

  build_dir = os.path.abspath(args.build_dir)
  target = args.target

  if not args.skip_build:
    if not args.skip_configure:
      configure(build_dir, target)

    build(build_dir)

  run_tests(build_dir, target)

if __name__ == "__main__":
  main()