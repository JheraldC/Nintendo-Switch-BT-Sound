"""Find an ARM bare-metal toolchain without a user-specific absolute path."""
import os
import shutil
from pathlib import Path

def tool(name):
    exe='arm-none-eabi-'+name+('.exe' if os.name=='nt' else '')
    configured=os.environ.get('DEVKITARM')
    if configured:
        candidate=Path(configured)/'bin'/exe
        if candidate.is_file():return candidate
    found=shutil.which(exe)
    if found:return Path(found)
    raise RuntimeError('Set DEVKITARM to your devkitARM directory or add arm-none-eabi tools to PATH')
