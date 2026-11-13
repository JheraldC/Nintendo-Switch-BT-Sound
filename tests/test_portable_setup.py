from pathlib import Path
import io,json,plistlib,tempfile,unittest,zipfile
from unittest import mock
from tools.partition_info import partition_info,bounds
from tools.portable_runtime import extract_archive,manifest

class PortableSetup(unittest.TestCase):
    def test_windows_partition_geometry(self):
        def run(args):return json.dumps({'FileSystem':'FAT32'} if 'Get-Volume' in args[-1] else {'Offset':2097152,'Size':51200000}).encode()
        self.assertEqual(partition_info('E:/',system='windows',run=run),(4096,104096))
        with self.assertRaises(ValueError):partition_info('E:/folder',system='windows',run=run)

    def test_macos_partition_geometry_and_rejects_subdirectory(self):
        with tempfile.TemporaryDirectory() as temp:
            data={'FilesystemType':'msdos','FilesystemName':'MS-DOS FAT32','MountPoint':temp,'WholeDisk':False,'PartitionMapPartitionOffset':2097152,'TotalSize':51200000,'DeviceBlockSize':512}
            run=lambda args:plistlib.dumps(data)
            self.assertEqual(partition_info(temp,system='darwin',run=run),(4096,104096))
            with self.assertRaises(ValueError):partition_info(Path(temp)/'folder',system='darwin',run=run)
            data['FilesystemName']='MS-DOS FAT16'
            with self.assertRaises(ValueError):partition_info(temp,system='darwin',run=run)

    def test_linux_geometry_and_fat16_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);node=root/'sys/sdb1';node.mkdir(parents=True)
            (node/'start').write_text('4096');(node/'size').write_text('100000')
            def run(args):
                if args[0]=='findmnt':return json.dumps({'filesystems':[{'source':'/dev/sdb1','target':str(root),'fstype':'vfat'}]}).encode()
                return b'vfat FAT32\n'
            self.assertEqual(partition_info(root,system='linux',run=run,sysfs=root/'sys'),(4096,104096))
            def bad(args):return b'vfat FAT16\n' if args[0]=='lsblk' else run(args)
            with self.assertRaises(ValueError):partition_info(root,system='linux',run=bad,sysfs=root/'sys')

    def test_bounds_require_alignment_range_and_sector_size(self):
        for args in ((0,512),(1,512),(512,513),(512,512,4096),(512,-512),(True,512)):
            with self.assertRaises(ValueError):bounds(*args)

    def test_zip_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);archive=root/'bad.zip'
            with zipfile.ZipFile(archive,'w') as z:z.writestr('../escape',b'bad')
            with self.assertRaises(ValueError):extract_archive(archive,root/'out')
            self.assertFalse((root/'escape').exists())

    def test_five_platforms_have_pinned_assets(self):
        data=manifest();self.assertEqual(len(data),5)
        for item in data.values():
            for asset in item.values():
                self.assertEqual(len(asset['sha256']),64)
                self.assertTrue(asset['url'].startswith('https://'))
                self.assertGreater(asset['bytes'],0)

if __name__=='__main__':unittest.main()
