import sys
import subprocess
import numpy as np
from matplotlib import pyplot as plt

argc = len(sys.argv)
fname = 'res.bin'
if argc > 1:
    if '.' in sys.argv[1]:
        fname = sys.argv[1]
    else:
        wavetable = sys.argv[1]
        wave = sys.argv[2] if argc > 2 else '0'
        nsamples = sys.argv[3] if argc > 3 else '512'
        subprocess.run(['x64/Debug/WvTable-test.exe', wavetable, wave, nsamples], check=True)

res = np.fromfile(fname, np.int32)
y = res.astype(float) * 2**-31
plt.figure(tight_layout=True)
plt.plot(y)
plt.grid()
plt.show()
