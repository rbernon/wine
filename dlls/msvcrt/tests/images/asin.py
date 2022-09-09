import matplotlib.pyplot as plt
import matplotlib as mpl
import numpy as np
import sympy as sp
import sys

plt.autoscale(False)
plt.style.use('dark_background')
plt.grid(True, alpha=0.1)
plt.figure(figsize=mpl.figure.figaspect(9 / 16), dpi=200)

x = sp.symbols('x')
f = sp.asin(x)

plt.xlabel(fr'${sp.latex(x)}$')
plt.ylabel(fr'${sp.latex(f)}$')

x = np.arange(-1, 1, 0.001)
f = np.arcsin(x)
plt.plot(x, f, '-', lw=1)

plt.savefig(sys.argv[1], format='png')
