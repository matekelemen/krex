# --- External Imports ---
import sympy
import numpy
import scipy
import scipy.io


numpy.set_printoptions(precision=1)

# Define symbols
x = sympy.Symbol("x")

# Define basis functions
linear_basis = [
    (1 - x) / 2,
    (1 + x) / 2,
    (1 - x) / 2,
    (1 + x) / 2
]

quadratic_basis = [
    x * (x - 1) / 2,
    x * (x + 1) / 2,
    x * (x - 1) / 2,
    x * (x + 1) / 2,
    1 - x * x,
    1 - x * x
]

# Define stiffness component functors
stiffness_generator = lambda left, right: sympy.integrate(sympy.diff(left) * sympy.diff(right), (x, -1, 1))
k_l = lambda i_left, i_right: stiffness_generator(linear_basis[i_left], linear_basis[i_right])
k_q = lambda i_left, i_right: stiffness_generator(quadratic_basis[i_left], quadratic_basis[i_right])

# Define unconstrained stiffnesses
linear_stiffness = numpy.array([
    [k_l(0, 0), k_l(0, 1),         0,         0],
    [k_l(1, 0), k_l(1, 1),         0,         0],
    [         0,        0, k_l(2, 2), k_l(2, 3)],
    [         0,        0, k_l(3, 2), k_l(3, 3)]
])

quadratic_stiffness = numpy.array([
    [k_q(0, 0), k_q(0, 1),         0,         0, k_q(0, 4),         0],
    [k_q(1, 0), k_q(1, 1),         0,         0, k_q(1, 4),         0],
    [        0,         0, k_q(2, 2), k_q(2, 3),         0, k_q(2, 5)],
    [        0,         0, k_q(3, 2), k_q(3, 3),         0, k_q(3, 5)],
    [k_q(4, 0), k_q(4, 1),         0,         0, k_q(4, 4),         0],
    [        0,         0, k_q(5, 2), k_q(5, 3),         0, k_q(5, 5)]
])

# Define the MPC relation matrices
quadratic_relations = numpy.array([
    [1, 0, 0, 0, 0, 0],
    [0, 1, 0, 0, 0, 0],
    [0, 1, 0, 0, 0, 0],
    [0, 0, 0, 1, 0, 0],
    [0, 0, 0, 0, 1, 0],
    [0, 0, 0, 0, 0, 1],
])

linear_relations = quadratic_relations[:4, :4]

# Compute the constrained stiffnesses
constrained_linear_stiffness = linear_relations.transpose().dot(linear_stiffness).dot(linear_relations).astype(float)
constrained_quadratic_stiffness = quadratic_relations.transpose().dot(quadratic_stiffness).dot(quadratic_relations).astype(float)

print(f"constrained linear stiffness:\n{constrained_linear_stiffness}")
print(f"constrained quadratic stiffness:\n{constrained_quadratic_stiffness}")

# Test restriction operators
#unconstrained_restriction_operator = numpy.array([
#    [1.0, 0.0, 0.0, 0.0, 0.5, 0.0],
#    [0.0, 1.0, 0.0, 0.0, 0.5, 0.0],
#    [0.0, 0.0, 1.0, 0.0, 0.0, 0.5],
#    [0.0, 0.0, 0.0, 1.0, 0.0, 0.5]
#])
#restriction_operator = linear_relations.transpose().dot(unconstrained_restriction_operator).dot(quadratic_relations)

restriction_operator = numpy.array([
    [1.0, 0.0, 0.0, 0.0, 0.5, 0.0],
    [0.0, 1.0, 1.0, 0.0, 0.5, 0.5],
    [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0, 1.0, 0.0, 0.5]
])
print(f"restriction operator:\n{restriction_operator}")
interpolation_operator = restriction_operator.transpose()

restricted_stiffness = restriction_operator.dot(constrained_quadratic_stiffness).dot(interpolation_operator)
print(f"restricted stiffness:\n{restricted_stiffness}")
print(f"restriction error:\n{numpy.linalg.norm(restricted_stiffness - constrained_linear_stiffness)}")

#interpolated_stiffness = interpolation_operator.dot(restricted_stiffness).dot(restriction_operator)
#print(interpolated_stiffness)
#scipy.io.mmwrite("interpolated_stiffness.mm", scipy.sparse.csr_matrix(interpolated_stiffness))
