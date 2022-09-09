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
f = (sp.asin(x) - x)/x

plt.xlabel(fr'$x$')
plt.ylabel(fr'$f(x)$')

X = np.arange(0.5, 0, -0.001)
F = (np.arcsin(X) - X) / X
plt.plot(X, F, '-', lw=1, label=rf'$f(x) = {sp.latex(f)}$')

plt.legend()
plt.savefig(sys.argv[1], format='png')
