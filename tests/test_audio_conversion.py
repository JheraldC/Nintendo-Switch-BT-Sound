from pathlib import Path
import hashlib,io,math,os,shutil,struct,subprocess,tempfile,unittest,wave,zipfile
from unittest import mock
from tools import audio_convert as a
from tools import install_sd as install

def wav(path,rate=22050,channels=1,seconds=.2):
    with wave.open(str(path),'wb') as f:
        f.setparams((channels,2,rate,0,'NONE','not compressed'))
        f.writeframes(b'\x01\x00'*int(rate*seconds)*channels)

class AudioConversion(unittest.TestCase):
    def test_compatible_wav_needs_no_ffmpeg_and_preserves_original(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=root/'original.wav';wav(source,48000,2)
            before=source.read_bytes()
            with mock.patch.object(a,'find_ffmpeg',side_effect=AssertionError('must not download')):
                info=a.convert_audio(source,root/'output.wav',download=True)
            self.assertEqual(source.read_bytes(),before)
            self.assertEqual((root/'output.wav').read_bytes(),before)
            self.assertEqual(info['pcm_bytes'],38400)

    def test_invalid_bounds_and_existing_output_do_not_write(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=root/'input.wav';wav(source)
            for start,duration in ((-1,1),(math.nan,1),(0,math.inf),(0,0),(0,11)):
                with self.assertRaises(ValueError):a.convert_audio(source,root/'new.wav',start,duration)
                self.assertFalse((root/'new.wav').exists())
            (root/'keep.wav').write_bytes(b'keep')
            with self.assertRaises(ValueError):a.convert_audio(source,root/'keep.wav')
            self.assertEqual((root/'keep.wav').read_bytes(),b'keep')
            with self.assertRaises(ValueError):a.convert_audio(source,source)

    def test_validator_rejects_invalid_alignment_extensible_and_truncated_pcm(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'test.wav';wav(p,48000,2);good=p.read_bytes()
            for data in (good[:-1],good[:20]+b'\xfe\xff'+good[22:],good[:32]+b'\x02\x00'+good[34:]):
                p.write_bytes(data)
                with self.assertRaises(ValueError):a.validate_wav(p)

    def test_corrupt_download_is_rejected_before_extract_or_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);wheel=root/'bad.whl';wheel.write_bytes(b'bad')
            with mock.patch.object(a,'checked_executable',side_effect=AssertionError('must not execute')):
                with self.assertRaises(ValueError):a.extract_runtime(wheel,root/'runtime')
            self.assertFalse((root/'runtime').exists())

    def test_cancelled_file_selection_never_converts_or_installs(self):
        import instalar
        with mock.patch('builtins.input',return_value='3'),mock.patch('tools.file_picker.select_audio_file',return_value=None),mock.patch.object(a,'convert_audio') as conversion,mock.patch.object(instalar,'install_files') as installation:
            instalar.audio_flow(Path('unused'),Path('unused-backup'))
            conversion.assert_not_called();installation.assert_not_called()

    @unittest.skipUnless(shutil.which('ffmpeg') or os.environ.get('FFMPEG_BINARY'),'FFmpeg required for integration')
    def test_real_mp3_conversion_and_custom_install_with_verified_backup(self):
        with tempfile.TemporaryDirectory(prefix='bootsound audio ') as temp:
            root=Path(temp);source=root/'música elegida.wav';wav(source,22050,1,12)
            mp3=root/'música elegida.mp3';exe=a.find_ffmpeg()
            subprocess.run([exe,'-nostdin','-v','error','-i',str(source),str(mp3)],check=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            original=hashlib.sha256(mp3.read_bytes()).hexdigest();converted=root/'custom.wav'
            info=a.convert_audio(mp3,converted,start=1,duration=10)
            self.assertEqual(info['seconds'],10)
            self.assertEqual(info['pcm_bytes'],1920000)
            self.assertEqual(hashlib.sha256(mp3.read_bytes()).hexdigest(),original)
            sd=root/'sd';(sd/'atmosphere').mkdir(parents=True);(sd/'bootloader').mkdir()
            (sd/'atmosphere/package3').write_bytes(b'keep package3')
            (sd/'bootloader/hekate_ipl.ini').write_bytes(b'keep boot config')
            config=sd/install.CONFIG;config.mkdir(parents=True)
            old=b'# Mis ajustes\r\naudio_file=yoshi_sound.wav\r\nother=value\r\n'
            (config/'config.ini').write_bytes(old)
            (config/'custom_sound.wav').write_bytes(b'old personal sound')
            actual,files,before=install.plan(sd,'audio',custom_wav=converted)
            self.assertEqual((config/'config.ini').read_bytes(),old)
            self.assertIn(b'other=value',files[install.CONFIG+'/config.ini'])
            install.apply(actual,files,before,root/'backup')
            self.assertEqual((root/'backup'/install.CONFIG/'custom_sound.wav').read_bytes(),b'old personal sound')
            self.assertEqual((root/'backup'/install.CONFIG/'config.ini').read_bytes(),old)
            self.assertEqual((sd/'atmosphere/package3').read_bytes(),b'keep package3')
            self.assertEqual((sd/'bootloader/hekate_ipl.ini').read_bytes(),b'keep boot config')
            self.assertEqual((config/'custom_sound.wav').read_bytes(),converted.read_bytes())
            with self.assertRaises(ValueError):a.convert_audio(mp3,root/'beyond.wav',start=30)
            self.assertFalse((root/'beyond.wav').exists())

if __name__=='__main__':unittest.main()
