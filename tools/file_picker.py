"""Selector nativo opcional; funciona también sin escritorio mediante una ruta."""
from pathlib import Path
import os
import shutil
import subprocess
import sys


def select_audio_file():
    try:
        if os.name=='nt':
            command=['powershell.exe','-NoProfile','-STA','-Command',
                "Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.OpenFileDialog; "
                "$d.Title='Selecciona tu sonido de arranque'; "
                "$d.Filter='Audio y vídeo|*.mp3;*.wav;*.flac;*.ogg;*.m4a;*.aac;*.opus;*.mp4;*.mkv|Todos los archivos|*.*'; "
                "if($d.ShowDialog() -eq 'OK'){[Console]::OutputEncoding=[Text.UTF8Encoding]::new();[Console]::Write($d.FileName)}"]
        elif sys.platform=='darwin':
            command=['osascript','-e','POSIX path of (choose file with prompt "Selecciona tu sonido de arranque")']
        elif shutil.which('zenity'):
            command=['zenity','--file-selection','--title=Selecciona tu sonido de arranque']
        elif shutil.which('kdialog'):
            command=['kdialog','--getopenfilename',str(Path.home()),'*','--title','Selecciona tu sonido de arranque']
        else:
            import tkinter
            from tkinter import filedialog
            root=tkinter.Tk();root.withdraw()
            try:return filedialog.askopenfilename(title='Selecciona tu sonido de arranque') or None
            finally:root.destroy()
        result=subprocess.run(command,capture_output=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        if result.returncode==0:
            return result.stdout.decode('utf-8',errors='strict').strip() or None
        # User cancellation is not an error and must not fall through to installation.
        return None
    except (ImportError,OSError,UnicodeError,RuntimeError):
        pass
    except Exception as exc:
        # tkinter raises TclError when no graphical session is available.
        if type(exc).__name__!='TclError':raise
    value=input('No hay selector gráfico disponible. Pega la ruta del audio (vacío cancela): ').strip()
    return value.strip('"') or None
