import mpl_toolkits.axes_grid1 as grid
import matplotlib.pyplot as plt
import bokeh.palettes as pal
import matplotlib as mpl
import pandas as pd
import numpy as np
import utils
import sys
import os

dir = sys.argv[2]
ranges = [
  (0x3dff5577, 0x3e7d5777),
  (0x3e7d5777, 0x3ef57744),
  (0x3ef57744, 0x3f000000),
]
step = 1

x = np.array([], dtype=np.float32)

for r in ranges:
  xmin, xmax = r

  data = utils.Float.from_repr(range(xmin, xmax, step))
  x = np.concatenate((x, data))

  print(f'Loaded {xmin:08x}-{xmax:08x}')

asin_madd = (np.arcsin(np.float64(x)) - np.float64(x)) / np.float64(x)
asin_madd = np.float32(asin_madd) * x + x

diff = np.float64(asin_madd) - np.arcsin(np.float64(x))

asin_ulp = utils.Float.ulp(np.arcsin(x))

diff_df = pd.DataFrame({'x': x, 'y': diff / asin_ulp})

plt.autoscale(False)
plt.style.use('dark_background')
fig = plt.figure(figsize=mpl.figure.figaspect(9 / 16))
grid = grid.ImageGrid(fig, 111, nrows_ncols=(1, 1), cbar_mode="each", cbar_size="2%")

grid[0].set_xlabel(r'$x$')
grid[0].set_ylabel(r'$ε$')

utils.plot(diff_df, cmap=list(pal.RdBu9), ax=grid[0], cbax=grid.cbar_axes[0], y_range=(0.4,0.6))

grid[0].grid(True, alpha=0.1)
grid[0].set_title("Theoretical * x + x operations 1ULP relative error")
plt.savefig(sys.argv[1], format='png')
