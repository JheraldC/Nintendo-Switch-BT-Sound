from pathlib import Path
import importlib.util, unittest
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

inc=load('inc','components/bootsplash/tools/boot_splash/png_to_boot_inc.py')
early=load('early','tools/patch_early_splash.py')

class Splash(unittest.TestCase):
    def test_bounds_and_alpha(self):
        for size in ((1281,720),(1280,721),(0,1),(1,0)):
            with self.subTest(size=size),self.assertRaises(ValueError):inc.generate_inc(Image.new('RGBA',size))
        for alpha in (-1,256):
            with self.assertRaises(ValueError):inc.generate_inc(Image.new('RGBA',(1,1)),alpha)
        text,rect=inc.generate_inc(Image.new('RGBA',(2,1),(1,2,3,4)),255)
        self.assertEqual(rect,(639,359,2,1));self.assertIn('0xFF010203',text)

    def test_early_region_and_alpha_roundtrip(self):
        base=b'PK31'+bytes([0x5a])*(early.SIZE-4)
        img=Image.new('RGBA',(1280,720),(19,71,113,128))
        img.putpixel((0,0),(1,2,3,4));img.putpixel((1279,719),(251,252,253,254))
        result=early.patch(base,img)
        self.assertEqual(len(result),len(base));self.assertEqual(result[:early.START],base[:early.START]);self.assertEqual(result[early.END:],base[early.END:])
        self.assertNotEqual(result[early.START:early.END],base[early.START:early.END])
        with self.assertRaises(ValueError):early.patch(bytes(early.SIZE),img)
        with self.assertRaises(ValueError):early.patch(base,Image.new('RGB',(1,1)))

if __name__=='__main__':unittest.main()
