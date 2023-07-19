import numpy as np
from matplotlib import pyplot as plt

res = np.fromfile('res.bin', np.int32)
y = res.astype(float) * 2**-31
plt.figure(tight_layout=True)
plt.plot(y)
plt.grid()
plt.show()
