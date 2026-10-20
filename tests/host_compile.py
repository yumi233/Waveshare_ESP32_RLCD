"""Compile small firmware host regressions on macOS/Linux or Visual C++."""
import os
from pathlib import Path
import shutil
import subprocess


def compile_cpp(source, exe, includes=()):
    source, exe = Path(source), Path(exe)
    if os.name != 'nt':
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise SystemExit('Install a C++17 compiler or set CXX')
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra',
                        *['-I' + str(p) for p in includes], str(source), '-o', str(exe)], check=True)
        return
    command = ['cl.exe', '/nologo', '/EHsc', '/std:c++17', '/utf-8',
               *['/I' + str(p) for p in includes], str(source),
               '/Fe:' + str(exe), '/Fo:' + str(exe.with_suffix('.obj'))]
    if shutil.which('cl.exe'):
        subprocess.run(command, cwd=source.parent, check=True)
        return
    vcvars = Path(os.environ.get('SYNA_VCVARS', r'D:\vs\VC\Auxiliary\Build\vcvars64.bat'))
    if not vcvars.is_file():
        raise SystemExit('Run in a Visual C++ developer prompt or set SYNA_VCVARS')
    batch = source.parent / 'build.cmd'
    batch.write_text('@echo off\nchcp 65001 >nul\nset VSCMD_SKIP_SENDTELEMETRY=1\ncall "' +
                     str(vcvars) + '" >nul\n' + subprocess.list2cmdline(command) +
                     '\nexit /b %errorlevel%\n', encoding='utf-8')
    subprocess.run(['cmd.exe', '/d', '/c', str(batch)], check=True)
