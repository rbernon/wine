import numpy as np

def plot(df, **kwargs):
  import datashader.mpl_ext as mpl
  import matplotlib.pyplot as plt
  import matplotlib.colors as clr
  import matplotlib.cm as cm
  import datashader as ds

  if not 'x_range' in kwargs:
    xmin, xmax = np.min(df['x']), np.max(df['x'])
    x_range = xmax - xmin if xmax != xmin else 1
    kwargs['x_range'] = (xmin - 0.05 * x_range, xmax + 0.05 * x_range)

  if not 'y_range' in kwargs:
    ymin, ymax = np.min(df['y']), np.max(df['y'])
    y_range = ymax - ymin if ymax != ymin else 1
    kwargs['y_range'] = (ymin - 0.05 * y_range, ymax + 0.05 * y_range)

  if 'aspect' not in kwargs:
    kwargs['aspect'] = 9 / 16

  xmin, xmax = kwargs['x_range']
  ymin, ymax = kwargs['y_range']
  kwargs['aspect'] *= (xmax - xmin) / (ymax - ymin)

  cbax = kwargs.get('cbax', None)
  del kwargs['cbax']

  plot = mpl.dsshow(df, ds.Point('x', 'y'), ds.count(), norm='eq_hist', **kwargs)
  if cbax is not None:
    cbar = plt.colorbar(cm.ScalarMappable(cmap=clr.ListedColormap(kwargs['cmap'])), cax=cbax);
    cbar.ax.set_yticks([])
    cbar.ax.text(0.45, 0.02, r'$-$', ha='center', va='center')
    cbar.ax.text(0.45, 0.98, r'$+$', ha='center', va='center')
    cbar.ax.set_ylabel('density', rotation=90)

class Float:
  def from_repr(data):
    import struct
    ints = struct.pack(f'{len(data)}i', *data)
    flts = struct.unpack(f'{len(data)}f', ints)
    return np.array(flts, dtype=np.float32)

  def repr(data):
    import struct
    flts = struct.pack(f'{len(data)}f', *data)
    ints = struct.unpack(f'{len(data)}i', flts)
    return np.array(ints, dtype=np.int32)

  def ulp(data):
    min_data = Float.from_repr(Float.repr(data) & 0x7f800000)
    ulp_data = Float.from_repr(Float.repr(min_data) + 1)
    return np.float64(ulp_data) - np.float64(min_data)
