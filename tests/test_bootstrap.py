from pathlib import Path
import hashlib,io,tempfile,unittest,zipfile
from unittest import mock
import bootstrap as b

class Bootstrap(unittest.TestCase):
    def archive(self,name='instalar.py'):
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w') as z:z.writestr('Nintendo-Switch-BT-Sound-'+b.COMMIT+'/'+name,b'print("fixture")\n')
        return out.getvalue()

    def context(self,data):
        return mock.patch.multiple(b,ARCHIVE_SHA256=hashlib.sha256(data).hexdigest(),ARCHIVE_BYTES=len(data))

    def test_download_integrity_cache_and_modified_code_rejection(self):
        data=self.archive()
        with tempfile.TemporaryDirectory() as temp,self.context(data),mock.patch.object(b.urllib.request,'urlopen',return_value=io.BytesIO(data)) as network:
            root=Path(temp);project=b.verified_project(root)
            self.assertEqual((project/'instalar.py').read_bytes(),b'print("fixture")\n')
            self.assertEqual(b.verified_project(root),project)
            self.assertEqual(network.call_count,1)
            (project/'instalar.py').write_text('changed')
            with self.assertRaises(ValueError):b.verified_project(root)

    def test_corrupt_download_and_traversal_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp,mock.patch.object(b.urllib.request,'urlopen',return_value=io.BytesIO(b'wrong')):
            with self.assertRaises(ValueError):b.verified_project(Path(temp))
        data=self.archive('../escape')
        with tempfile.TemporaryDirectory() as temp,self.context(data),mock.patch.object(b.urllib.request,'urlopen',return_value=io.BytesIO(data)):
            with self.assertRaises(ValueError):b.verified_project(Path(temp))
            self.assertFalse((Path(temp)/'escape').exists())

    def test_parallel_launch_is_blocked_and_owned_lock_is_cleaned(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            with mock.patch.object(b,'cache_root',return_value=root),mock.patch.object(b,'verified_project',return_value=root),mock.patch.object(b.subprocess,'call',return_value=0):
                self.assertEqual(b.main(),0)
                self.assertFalse((root/'installer.lock').exists())
                (root/'installer.lock').write_text('other process')
                with self.assertRaises(ValueError):b.main()
                self.assertEqual((root/'installer.lock').read_text(),'other process')

if __name__=='__main__':unittest.main()
