"""编译实际固件路径，仅替代硬件边界；不连接设备、不输出持久编译产物。"""
from pathlib import Path
import importlib.util
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
if os.environ.get("CXX"):
    compiler = shlex.split(os.environ["CXX"])
elif shutil.which("g++"):
    compiler = [shutil.which("g++")]
elif shutil.which("clang++"):
    compiler = [shutil.which("clang++")]
elif importlib.util.find_spec("ziglang"):
    compiler = [sys.executable, "-m", "ziglang", "c++"]
    if os.name == "nt":
        compiler += ["-target", "x86_64-windows-gnu"]
else:
    raise SystemExit("需要C++编译器：设置CXX或安装g++/clang++/python -m pip install ziglang")
sources = ["test/host/calibration_test.cpp"] + [f"src/{name}.cpp" for name in (
    "TempCalibration", "ADC", "PWM", "eeprom_flash", "EC11", "ui", "MIOT", "Tick_IRQ"
)]
with tempfile.TemporaryDirectory(prefix="esp32-calibration-") as directory:
    executable = Path(directory) / ("calibration.exe" if os.name == "nt" else "calibration")
    command = compiler + ["-std=c++17", "-O0", "-g", "-funsigned-char", "-Itest/host/support", "-Iinclude",
                          "-include", "test/host/support/Harness.h", *sources, "-o", str(executable)]
    subprocess.run(command, cwd=root, check=True)
    subprocess.run([str(executable)], cwd=root, check=True)
