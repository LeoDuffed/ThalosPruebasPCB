#!/usr/bin/env python3
"""Localizar herramientas existentes; no instala ni descarga programas."""
import glob
import os
from pathlib import Path
import shutil
import sys

names = {
    "gcc": "arm-none-eabi-gcc",
    "objcopy": "arm-none-eabi-objcopy",
    "size": "arm-none-eabi-size",
    "objdump": "arm-none-eabi-objdump",
    "nm": "arm-none-eabi-nm",
    "programmer": "STM32_Programmer_CLI",
}
kind = sys.argv[1]
name = names[kind]
paths = []
variable = "CUBE_PROGRAMMER_BIN" if kind == "programmer" else "TOOLCHAIN_BIN"
if os.environ.get(variable):
    paths.append(str(Path(os.environ[variable]) / name))
if shutil.which(name):
    paths.append(shutil.which(name))
plugins = "/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins"
pattern = ("com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*/tools/bin/"
           if kind == "programmer"
           else "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/")
paths += sorted(glob.glob(plugins + "/" + pattern + name), reverse=True)
for candidate in paths:
    if Path(candidate).is_file() and os.access(candidate, os.X_OK):
        print(candidate)
        sys.exit(0)
print("No encuentro " + name + ". Revisa STM32CubeIDE o " + variable + ".", file=sys.stderr)
sys.exit(1)
