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
  (0x38800000, 0x39000000),
  (0x39000000, 0x39800000),
  (0x39800000, 0x39ffffff),
  (0x39ffffff, 0x3a7ffffd),
  (0x3a7ffffd, 0x3afffff5),
  (0x3afffff5, 0x3b7fffd5),
  (0x3b7fffd5, 0x3bffff55),
  (0x3bffff55, 0x3c7ffd55),
  (0x3c7ffd55, 0x3cfff555),
  (0x3cfff555, 0x3d7fd557),
  (0x3d7fd557, 0x3dff5577),
  (0x3dff5577, 0x3e7d5777),
  (0x3e7d5777, 0x3ef57744),
  (0x3ef57744, 0x3f000000),
  (0x3f000000, 0x3f576aa5),
  (0x3f576aa5, 0x3f800000),
]
step = 1

x = np.array([], dtype=np.float32)
wine_asinf = np.array([], dtype=np.float32)

for r in ranges:
  xmin, xmax = r

  data = utils.Float.from_repr(range(xmin, xmax, step))
  x = np.concatenate((x, data))
  data = np.fromfile(open(f'{dir}/asinf-{xmin:08x}-{xmax:08x}-0-wine.dat', 'rb'), dtype=np.float32)[::step]
  wine_asinf = np.concatenate((wine_asinf, data))

  print(f'Loaded {xmin:08x}-{xmax:08x}')

diff = np.float64(wine_asinf) - np.arcsin(np.float64(x))
asin_ulp = utils.Float.ulp(np.arcsin(x))

diff_df = pd.DataFrame({'x': x, 'y': diff / asin_ulp})

plt.autoscale(False)
plt.style.use('dark_background')
fig = plt.figure(figsize=mpl.figure.figaspect(9 / 16))
grid = grid.ImageGrid(fig, 111, nrows_ncols=(1, 1), cbar_mode="each", cbar_size="2%")

grid[0].set_xlabel(r'$x$')
grid[0].set_ylabel(r'$ε$')

utils.plot(diff_df, cmap=list(pal.RdBu9), ax=grid[0], cbax=grid.cbar_axes[0], y_range=(-1,+1))

grid[0].grid(True, alpha=0.1)
grid[0].set_title("Wine asinf(x) 1ULP relative error")
plt.savefig(sys.argv[1], format='png')
