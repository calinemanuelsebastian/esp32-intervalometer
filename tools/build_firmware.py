#!/usr/bin/env python3
"""Compile one edition and audit its normal timer paths; never flash hardware."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('edition', choices=('OLED', 'TFT'))
parser.add_argument('--arduino-cli', default='arduino-cli')
parser.add_argument('--config-file', type=Path)
parser.add_argument('--build-root', type=Path, default=root / 'build')
parser.add_argument('--objdump', required=True, type=Path,
                    help='xtensa-esp32-elf-objdump from the pinned ESP32 toolchain')
args = parser.parse_args()
cli = shutil.which(args.arduino_cli)
if not cli:
    parser.error('Arduino CLI executable not found: ' + args.arduino_cli)
objdump = args.objdump.resolve()
if not objdump.is_file():
    parser.error('objdump not found: ' + str(objdump))
build = args.build_root.resolve() / args.edition
command = [cli]
if args.config_file:
    command += ['--config-file', str(args.config_file.resolve())]
command += ['compile', '--fqbn', 'esp32:esp32:esp32', '--warnings', 'all',
            '--build-path', str(build), str(root / args.edition / 'R7Intervalometer')]
subprocess.run(command, check=True, cwd=root)
subprocess.run([sys.executable, str(root / 'tools/verify_timer_iram.py'),
                str(objdump), str(build / 'R7Intervalometer.ino.elf')], check=True, cwd=root)
print('PASS: ' + args.edition + ' target build and IRAM audit.')
