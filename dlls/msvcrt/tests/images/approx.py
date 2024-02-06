import numpy.polynomial.polynomial as poly
import scipy.optimize as opt
import numpy as np
import scipy as sp
import polyrat
import ctypes
import utils
import sys
import os

np.set_printoptions(floatmode='unique')

dir = sys.argv[2]
test = int(sys.argv[3])
ranges = [
  # (0x38800000, 0x39000000, 0),
  # (0x39000000, 0x39800000, 0),
  # (0x39800000, 0x39ffffff, 0),
  # (0x39ffffff, 0x3a7ffffd, 0),
  # (0x3a7ffffd, 0x3afffff5, 0),
  # (0x3afffff5, 0x3b7fffd5, 0),
  # (0x3b7fffd5, 0x3bffff55, 0),
  # (0x3bffff55, 0x3c7ffd55, 0),
  # (0x3c7ffd55, 0x3cfff555, 0),
  # (0x3cfff555, 0x3d7fd557, 0),
  # (0x3d7fd557, 0x3dff5577, 0),
  # (0x3dff5577, 0x3e7d5777, 0),
  # (0x3e7d5777, 0x3ef57744, 0),
  (0x3ef57744, 0x3f000000, 0),
  # (0x3f000000, 0x3f576aa5, 0),
  # (0x3f576aa5, 0x3f800000, 0),
]
step = 10

x = [[],[],[],[]]
wine_asinf = [[],[],[],[]]
msvc_asinf = [[],[],[],[]]

for xmin, xmax, mode in ranges:
  if test < 3 and xmin == 0x3f000000: break
  x[mode] += [utils.Float.from_repr(range(xmin, xmax, step))]
  wine_asinf[mode] += [np.fromfile(open(f'{dir}/asinf-{xmin:08x}-{xmax:08x}-{mode}-wine.dat', 'rb'), dtype=np.float32)[::step]]
  msvc_asinf[mode] += [np.fromfile(open(f'{dir}/asinf-{xmin:08x}-{xmax:08x}-{mode}-msvc.dat', 'rb'), dtype=np.float32)[::step]]
  print(f'Loaded {xmin:08x}-{xmax:08x}')

x = np.concatenate(x[0])
wine_asinf = np.concatenate(wine_asinf[0])
msvc_asinf = np.concatenate(msvc_asinf[0])
asin_ulp = utils.Float.ulp(msvc_asinf)

if test < 3:
  X = np.float64(x)
  msvc_core = np.float32((np.arcsin(X) - X) / X) * x + x
  msvc_cerr = np.float64(msvc_core) - np.arcsin(X)
  msvc_core = (np.float64(msvc_asinf) - msvc_cerr - X) / X

  if test == 1:
    Z = np.float64(x**2)
    P = np.polyfit(Z, msvc_core, 6)

    poly_core = np.polyval(P, Z)
    poly_asinf = np.float32(poly_core) * x + x

  elif test == 2:
    Z = np.float64(x**2).reshape(-1, 1)
    P = polyrat.StabilizedSKRationalApproximation(8, 1)
    P.fit(Z, msvc_core)

    poly_core = P(Z)
    poly_asinf = np.float32(poly_core) * x + x

else:
  dtype = np.float32
  n, m = 6, 0
  Pf = [3/40,1/6,0]
  Qf = [1]

  z = np.where(x <= 0.5, x**2, (1 - x) / 2)
  s = np.where(x <= 0.5, x, np.sqrt(z))

  f = utils.Float.from_repr(utils.Float.repr(s) & 0xffff0000)
  c = (z - f**2) / (s + f)

  def asinf_R(A):
      p = po = pe = q = qo = qe = dtype(0)
      P = np.array(np.concatenate((A[:n], Pf), dtype=dtype))
      Q = np.array(np.concatenate((A[n:], Qf), dtype=dtype))
      for pn in P: p = pn + z * p
      for qn in Q: q = qn + z * q
      # for pn in P[-1::-2][::-1]: pe = pn + (z*z) * pe
      # for pn in P[-2::-2][::-1]: po = pn + (z*z) * po
      # p = pe + z * po
      # for qn in Q[-1::-2][::-1]: qe = qn + (z*z) * qe
      # for qn in Q[-2::-2][::-1]: qo = qn + (z*z) * qo
      # q = qe + z * qo
      return np.float32(p / q)
  def asinf_R_jac(A):
      p = po = pe = q = qo = qe = dtype(0)
      P = np.array(np.concatenate((A[:n], Pf), dtype=dtype))
      Q = np.array(np.concatenate((A[n:], Qf), dtype=dtype))
      for pn in P: p = pn + z * p
      for qn in Q: q = qn + z * q
      # for pn in P[-1::-2][::-1]: pe = pn + (z*z) * pe
      # for pn in P[-2::-2][::-1]: po = pn + (z*z) * po
      # p = pe + z * po
      # for qn in Q[-1::-2][::-1]: qe = qn + (z*z) * qe
      # for qn in Q[-2::-2][::-1]: qo = qn + (z*z) * qo
      # q = qe + z * qo

      J = np.full((m + n, len(z)), 1, dtype=dtype)
      for i in range(len(Qf)): J[n:] *= z
      for i in range(m): J[n:n+i] *= z
      J[n:] *= -p / q

      for i in range(len(Pf)): J[:n] *= z
      for i in range(n): J[:i] *= z
      J /= q

      # for i in range(n): J[0:0+1+i] *= z
      # for i in range(len(Pf)): J[:n] *= z
      # for i, qn in enumerate(Q[:m]): J[n:n+1+i] *= z
      # for i in range(len(Pf) - 1): J[:n] *= z
      # for i, qn in enumerate(Q[m:-1]): J[n:] *= z

      return np.transpose(J)

  def asinf_lo(A):
      return asinf_R(A) * x + x
  def asinf_lo_jac(A):
      return asinf_R_jac(A)

  def asinf_hi(A):
      pio4_hi = np.float32(0.785398125648)
      pio2_lo = np.float32(7.54978941586e-08)
      return pio4_hi - (2 * s * asinf_R(A) - (pio2_lo - 2 * c) - (pio4_hi - 2 * f))
  def asinf_hi_jac(A):
      return -2 * asinf_R_jac(A)

  def asinf(A):
      return np.where(x <= 0.5, asinf_lo(A), asinf_hi(A))
  def asin_jac(A):
      return np.where(x.reshape(-1,1) <= 0.5, asinf_lo_jac(A), asinf_hi_jac(A))

  def err(A):
      return (np.float64(asinf(A)) - np.float64(msvc_asinf)) / asin_ulp
  def err_jac(A):
      return np.float64(asin_jac(A)) / asin_ulp.reshape(-1,1)

  def min_err(A):
      return np.sum(err(A)**2)
  def min_err_jac(A):
      return np.sum(2 * err_jac(A) * err(A).reshape(-1,1), axis=0)

  Pi, Qi = [], [1]
  # Pi = [-1.4500209e-06, -2.3519241e-03, -1.2172896e-02, -4.8798278e-02, 7.0307620e-02, 1/6, 0]
  # Qi = [1.0557944e-06, -5.4810822e-01, -2.8148992e-02, 1]

  A = np.float64([0] * (n + m))
  A[0:][max(0, n - len(Pi)):n] = Pi[max(0, len(Pi) - n):len(Pi)]
  A[n:][max(0, m - len(Qi)):m] = Qi[max(0, len(Qi) - m):len(Qi)]
  A = dtype(A)

  if test == 3:
    out = opt.leastsq(err, A, Dfun=err_jac)
    A = dtype(out[0])
    print(out)
    print(repr(A))
  elif test == 4:
    print(min_err(A), repr(A))

    out = opt.least_squares(err, A, jac=err_jac, bounds=np.transpose([(-1, 1)] * (m+n)), verbose=2)
    A = dtype(out.x)
    print(out)
    print(min_err(A), repr(A))

    # out = opt.minimize(min_err, A, method='Nelder-Mead', tol=1e-12, options={'disp':True}, bounds=np.array([(-1, 1)] * (m+n)))
    # A = dtype(out.x)
    # print(out)
    # print(min_err(A), repr(A))

  off, step = 0, 1
  poly_asinf = asinf(A)


asin = np.arcsin(np.float64(x))
poly_err = np.float64(poly_asinf) - asin
wine_err = np.float64(wine_asinf) - asin
msvc_err = np.float64(msvc_asinf) - asin

print('poly vs wine', len(x[poly_asinf != wine_asinf]), np.max(np.abs((np.float64(poly_asinf) - np.float64(wine_asinf)) / asin_ulp)))
print('poly vs msvc', len(x[poly_asinf != msvc_asinf]), np.max(np.abs((np.float64(poly_asinf) - np.float64(msvc_asinf)) / asin_ulp)))
print('wine vs msvc', len(x[wine_asinf != msvc_asinf]), np.max(np.abs((np.float64(wine_asinf) - np.float64(msvc_asinf)) / asin_ulp)))

print('poly vs asin', len(x[poly_asinf != np.float32(asin)]), np.max(np.abs(poly_err / asin_ulp)))
print('wine vs asin', len(x[wine_asinf != np.float32(asin)]), np.max(np.abs(wine_err / asin_ulp)))
print('msvc vs asin', len(x[msvc_asinf != np.float32(asin)]), np.max(np.abs(msvc_err / asin_ulp)))
