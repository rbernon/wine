import matplotlib.pyplot as plt
import matplotlib as mpl
import sympy as sp
import numpy as np
import polyrat
import sys

plt.autoscale(False)
plt.style.use('dark_background')
plt.grid(True, alpha=0.1)
fig, ax1 = plt.subplots(figsize=mpl.figure.figaspect(9 / 16), dpi=200)
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


# Polyrat expansion of x^2

X2 = (X * X).reshape(-1, 1)
p = polyrat.LinearizedRationalApproximation(1, 1, Basis=polyrat.MonomialPolynomialBasis)
p.fit(X2, F)

def polyratcoefs(p):
  a = p.eval(0)[0]
  return [a, p.eval(1)[0] - a]

P = np.polynomial.Polynomial(polyratcoefs(p.numerator))
Q = np.polynomial.Polynomial(polyratcoefs(p.denominator))
print(f'P(x) = {P}')
print(f'Q(x) = {Q}')

R = P(X * X) / Q(X * X)
plots += ax1.plot(X, R, '-', lw=1, label=rf'$R(x²) = P(x²) / Q(x²)$')


plots += ax2.plot(X, (F - R), '-', color='C3', lw=1, label=r'$ε = f(x) - R(x²)$')


ax1.legend(plots, [l.get_label() for l in plots], loc=0)
plt.savefig(sys.argv[1], format='png')
