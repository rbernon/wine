from bokeh.palettes import RdBu9, PiYG9
from collections import OrderedDict as odict
from datashader.utils import export_image
from scipy.ndimage.filters import maximum_filter1d
import datashader as ds
import datashader.transfer_functions as tf
import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import struct

build = os.path.expanduser('~/Code/build-wine')
step = 1
ranges = [
 #  (0x38800000, 0x39500000),
  # (0x39500000, 0x3a200000),
  # (0x3a200000, 0x3af00000),
  # (0x3af00000, 0x3bc00000),
  # (0x3bc00000, 0x3c900000),
  # (0x3c900000, 0x3d600000),
  # (0x3d600000, 0x3e300000),
  (0x3e300000, 0x3f000000),
#  (0x3f000000, 0x3f100000),
#  (0x3f100000, 0x3f200000),
#  (0x3f200000, 0x3f300000),
#  (0x3f300000, 0x3f400000),
#  (0x3f400000, 0x3f500000),
#  (0x3f500000, 0x3f600000),
#  (0x3f600000, 0x3f700000),
#  (0x3f700000, 0x3f800000),
]

for xmin, xmax, mode in [(mn, mx, m) for mn, mx in ranges for m in [0,1,2,3]]:
  np.random.seed(1)

  def plot(x, y, name, color, y_range=None):
    data = pd.DataFrame(odict([('x', x), ('y', y)]))
    cvs = ds.Canvas(plot_width=1600, plot_height=900, y_range=y_range)
    agg = cvs.points(data, 'x','y')
    img = tf.shade(agg, cmap=list(color))
    export_image(img, f'{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-{name}', background='black')

  beg = 0
  end = 0x3e7d5777-xmin

  # beg = 0x3e7d5777-xmin
  # end = beg+0x3ee74e97-0x3e7d5777

  with open(f'{build}/x-{xmin:08x}-{xmax:08x}.dat', 'wb') as out:
    for i in range(xmin,xmax):
      out.write(struct.pack('i',i))
  xf = np.fromfile(open(f'{build}/x-{xmin:08x}-{xmax:08x}.dat', 'rb'), dtype=np.float32)[beg:end:step]
  x = np.float64(xf)
  os.remove(f'{build}/x-{xmin:08x}-{xmax:08x}.dat')

  ms_asinf = np.fromfile(open(f"{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-windows.dat", 'rb'), dtype=np.float32)[beg:end:step]
  wn_asinf = np.fromfile(open(f"{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-wine.dat", 'rb'), dtype=np.float32)[beg:end:step]
  md_asinf = np.fromfile(open(f"{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-bf-maddx.dat", 'rb'), dtype=np.float32)[beg:end:step]
  bf_asin = np.fromfile(open(f"{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-bf-asin.dat", 'rb'), dtype=np.float64)[beg:end:step]
  bf_asinf = np.fromfile(open(f"{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-bf-asinf.dat", 'rb'), dtype=np.float32)[beg:end:step]
  np_asin = np.arcsin(x)
  np_asinf = np.arcsin(xf)
  asin = bf_asin
  asinf = bf_asinf

  diff = np.where(ms_asinf != wn_asinf)
  print(f'{len(diff[0])} differences')

  core = np.float32((asin-x)/x/x/x-1./6.)

  round_err = (np.float64(asinf) - asin)**2
  core_err = (np.float64((np.float32(1./6.)+core) * xf * xf * xf + xf) - asin)**2
  np_err = (np.float64(np_asinf) - asin)**2
  bf_err = (np.float64(bf_asinf) - asin)**2
  ms_err = (np.float64(ms_asinf) - asin)**2
  wn_err = (np.float64(wn_asinf) - asin)**2
  md_err = (np.float64(md_asinf) - asin)**2

  # core_err /= round_err
  # bf_err /= round_err
  # np_err /= round_err
  # ms_err /= round_err
  # wn_err /= round_err

  # idx = np.where((ms_err <= np.percentile(ms_err, 99.999)) &
  #                (wn_err <= np.percentile(wn_err, 99.999)))
  # x = x[idx]
  # round_err = round_err[idx]
  # ms_err = ms_err[idx]
  # wn_err = wn_err[idx]

  # K = 50000
  # max_err = maximum_filter1d(bf_err, size=K, mode='nearest')
  # ms_err = 1 + (ms_err - 1) / max_err
  # np_err = 1 + (np_err - 1) / max_err

  ymin = np.amin([ms_err, wn_err]) # round_err, core_err
  ymax = np.amax([ms_err, wn_err]) # round_err, core_err

  # plot(x, round_err, 'round', RdBu9) #, y_range=(ymin, ymax))
  # plot(x, core_err, 'core', RdBu9, y_range=(ymin, ymax))
  plot(x, ms_err, 'windows', RdBu9, y_range=(ymin, ymax))
  plot(x, wn_err, 'wine', PiYG9, y_range=(ymin, ymax))
  plot(x, np_err, 'numpy', PiYG9, y_range=(ymin, ymax))
  # plot(x, md_err, 'maddx', PiYG9, y_range=(ymin, ymax))

  # # agg = cvs.points(dists['rd'],'x','y')
  # # img = tf.shade(agg, cmap=list(PiYG9))
  # # export_image(img, f'{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-round', background='black')

  # # agg = cvs.points(dists['bf'],'x','y')
  # # img = tf.shade(agg, cmap=list(PiYG9))
  # # export_image(img, f'{build}/asinf-{xmin:08x}-{xmax:08x}-{mode}-maddx', background='black')

  # dists = {
  # #  'bf': pd.DataFrame(odict([('x', x[diff]), ('y', bf_err[diff]+2)])),
  #   'ms': pd.DataFrame(odict([('x', x[diff]), ('y', ms_err[diff])])),
  #   'wn': pd.DataFrame(odict([('x', x[diff]), ('y', wn_err[diff]-3)])),
  #   'np': pd.DataFrame(odict([('x', x[diff]), ('y', np_err[diff]-6)])),
  # }
  # df = pd.concat(dists, ignore_index=True)

  # cvs = ds.Canvas(plot_width=1600, plot_height=900, x_axis_type='linear', y_axis_type='linear')
  # agg = cvs.points(df,'x','y')
  # img = tf.shade(agg, cmap=list(RdBu9))

  # # export_image(img, f'{build}/err-{xmin:08x}-{xmax:08x}', background='black')






step = 1
ranges = [
#  (0x32800000, 0x34100000),
#  (0x34100000, 0x35a00000),
#  (0x35a00000, 0x37300000),
#  (0x37300000, 0x38c00000),
#  (0x38c00000, 0x3a500000),
#  (0x3a500000, 0x3be00000),
#  (0x3be00000, 0x3d700000),
#  (0x3d700000, 0x3f000000),
#  (0x3f000000, 0x3f100000),
#  (0x3f100000, 0x3f200000),
#  (0x3f200000, 0x3f300000),
#  (0x3f300000, 0x3f400000),
#  (0x3f400000, 0x3f500000),
#  (0x3f500000, 0x3f600000),
#  (0x3f600000, 0x3f700000),
#  (0x3f700000, 0x3f800000),
]

for xmin, xmax, mode in [(mn, mx, m) for mn, mx in ranges for m in [0,1,2,3]]:
  np.random.seed(1)

  def plot(x, y, name, color, y_range=None):
    data = pd.DataFrame(odict([('x', x), ('y', y)]))
    cvs = ds.Canvas(plot_width=1600, plot_height=900, y_range=y_range)
    agg = cvs.points(data, 'x','y')
    img = tf.shade(agg, cmap=list(color))
    export_image(img, f'{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-{name}', background='black')

  with open(f'{build}/x-{xmin:08x}-{xmax:08x}.dat', 'wb') as out:
    for i in range(xmin,xmax):
      out.write(struct.pack('i',i))
  xf = np.fromfile(open(f'{build}/x-{xmin:08x}-{xmax:08x}.dat', 'rb'), dtype=np.float32)[::step]
  x = np.float64(xf)
  os.remove(f'{build}/x-{xmin:08x}-{xmax:08x}.dat')

  ms_acosf = np.fromfile(open(f"{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-windows.dat", 'rb'), dtype=np.float32)[::step]
  wn_acosf = np.fromfile(open(f"{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-wine.dat", 'rb'), dtype=np.float32)[::step]
  bf_acos = np.fromfile(open(f"{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-bf-acos.dat", 'rb'), dtype=np.float64)[::step]
  bf_acosf = np.fromfile(open(f"{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-bf-acosf.dat", 'rb'), dtype=np.float32)[::step]
  np_acos = np.arccos(x)
  np_acosf = np.arccos(xf)
  acos = bf_acos
  acosf = bf_acosf

  diff = np.where(ms_acosf != wn_acosf)
  print(f'{len(diff[0])} differences')

  core = np.float32((np.pi / 2. - acos - x) / x)

  round_err = (np.float64(acosf) - acos)**2
  # core_err = (np.float64(np.pi / 2. - np.float64(xf + xf * core)) - acos)**2
  np_err = (np.float64(np_acosf) - acos)**2
  ms_err = (np.float64(ms_acosf) - acos)**2
  wn_err = (np.float64(wn_acosf) - acos)**2

  # round_err = maximum_filter1d(round_err, size=K, mode='nearest')

  # np_err /= round_err
  # ms_err /= round_err
  # wn_err /= round_err

  # K = 50000
  # max_err = maximum_filter1d(bf_err, size=K, mode='nearest')
  # ms_err = 1 + (ms_err - 1) / max_err
  # np_err = 1 + (np_err - 1) / max_err

  ymin = np.amin([ms_err, wn_err, round_err])
  ymax = np.amax([ms_err, wn_err, round_err])

  plot(x, round_err, 'round', RdBu9, y_range=(ymin, ymax))
  plot(x, ms_err, 'windows', RdBu9, y_range=(ymin, ymax))
  plot(x, wn_err, 'wine', PiYG9, y_range=(ymin, ymax))

  # agg = cvs.points(dists['cr'],'x','y')
  # img = tf.shade(agg, cmap=list(PiYG9))
  # export_image(img, f'{build}/acosf-{xmin:08x}-{xmax:08x}-{mode}-core', background='black')
