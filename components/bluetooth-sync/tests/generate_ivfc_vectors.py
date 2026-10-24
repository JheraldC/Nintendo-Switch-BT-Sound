"""Independent known answers from Python hashlib/hmac; synthetic seeds only."""
from pathlib import Path
import hashlib,hmac,struct
root=Path(__file__).resolve().parent/'corpus';root.mkdir(exist_ok=True)
with (root/'ivfc-hmac-vectors.bin').open('wb') as f:
    f.write(struct.pack('<I',64))
    for i in range(64):
        seed=bytes(32) if i==0 else bytes([255])*32 if i==1 else hashlib.sha256(('synthetic IVFC seed '+str(i)).encode()).digest()
        f.write(seed)
        for level in range(4):
            label=('HierarchicalIntegrityVerificationStorage::'+('Master' if level==0 else 'L'+str(level))).encode()
            f.write(hmac.new(label,seed,hashlib.sha256).digest())
print('Independent IVFC HMAC vectors: 64 synthetic seeds x 4 labels')
