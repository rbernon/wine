import matplotlib.pyplot as plt
import matplotlib as mpl
import sympy as sp
import numpy as np
import sys

plt.autoscale(False)
plt.style.use('dark_background')
plt.grid(True, alpha=0.1)
fig, ax1 = plt.subplots(figsize=mpl.figure.figaspect(9 / 16))
ax2 = ax1.twinx()
plots = []

ax1.set_xlabel(r'$x$')
ax1.set_ylabel(r'$f(x)$')
ax2.set_ylabel(r'$ε$')


x = sp.symbols('x')
f = (sp.asin(x) - x) / x

X = np.arange(0.5, 0, -0.001)
F = (np.arcsin(X) - X) / X
plots += ax1.plot(X, F, '-', lw=1, label=rf'$f(x) = {sp.latex(f)}$')


# Taylor expansion of x^2
x = sp.symbols('x^2')
f = (sp.asin(sp.sqrt(x)) - sp.sqrt(x)) / sp.sqrt(x)
p = sp.series(f, x, x0=1/8, n=3).removeO()
ps = [float(v) for v in reversed(sp.poly(p).all_coeffs())]

P = np.polynomial.Polynomial(ps)
print(f'P(x) = {P}')

P = P(X * X)
plots += ax1.plot(X, P, '-', lw=1, label=rf'$P(x²)$')


plots += ax2.plot(X, (F - P), '-', color='C2', lw=1, label=r'$ε = f(x) - P(x²)$')


ax1.legend(plots, [l.get_label() for l in plots], loc=0)
plt.savefig(sys.argv[1], format='png')
