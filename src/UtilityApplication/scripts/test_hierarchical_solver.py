#           +-----+-----+ (master) <- MPC -> (slave) +-----+-----+   <==
#           0     2     1                            3     5     4    F
#           | element 0 |                            | element 1 |

# --- External Imports ---
import sympy
import numpy
import scipy
import scipy.io

# --- STD Imports ---
import typing


numpy.set_printoptions(precision=1)

# Define workflow for constructing an unconstrained system matrix
def MakeStiffnessComponent(left_basis_function: typing.Callable[[float],float],
                           right_basis_function: typing.Callable[[float],float],
                           variable: sympy.Symbol) -> float:
    return sympy.integrate(sympy.diff(left_basis_function) * sympy.diff(right_basis_function), (variable, -1, 1))

def MakeStiffness(basis_functions: list[typing.Callable],
                  variable: sympy.Symbol) -> numpy.ndarray:
    return numpy.array([
        [MakeStiffnessComponent(f_i, f_j, variable) for f_j in basis_functions] for f_i in basis_functions
    ])

def AssembleStiffness(element_stiffness: numpy.ndarray,
                      element_count: int) -> numpy.ndarray:
    dofs_per_element = element_stiffness.shape[0]
    system_size = element_count * dofs_per_element
    stiffness = numpy.zeros((system_size, system_size))
    for i_element in range(element_count):
        begin = i_element * dofs_per_element
        end = begin + dofs_per_element
        stiffness[begin:end, begin:end] = element_stiffness.copy()
    return stiffness

def MakeRelations(system_size: int,
                  masters: list[int],
                  slaves: list[int]) -> numpy.ndarray:
    relations = numpy.identity(system_size)
    if len(masters) != len(slaves):
        raise ValueError(f"master count ({len(masters)}) must match slave count ({len(slaves)})")

    for i_master, i_slave in zip(masters, slaves):
        if i_master in slaves:
            raise ValueError(f"master {i_master} is also a slave")
        if i_slave in masters:
            raise ValueError(f"slave {i_slave} is also a master")

        relations[i_slave, :] = numpy.zeros((1, system_size))
        relations[i_slave, i_master] = 1.0
    return relations

# Init
x = sympy.Symbol("x")
element_count = 2

# Define unconstrained linear system
linear_basis = [(1 - x) / 2,
                (1 + x) / 2 ]
unconstrained_linear_element_stiffness = MakeStiffness(linear_basis, x)
unconstrained_linear_stiffness = AssembleStiffness(unconstrained_linear_element_stiffness, element_count)

# Define unconstrained quadratic system
quadratic_basis = [x * (x - 1) / 2,
                   x * (x + 1) / 2,
                   1 - x * x       ]
unconstrained_quadratic_element_stiffness = MakeStiffness(quadratic_basis, x)
unconstrained_quadratic_stiffness = AssembleStiffness(unconstrained_quadratic_element_stiffness, element_count)

# Define MPCs
quadratic_system_size = unconstrained_quadratic_stiffness.shape[0]
masters = [1]
slaves = [3]
quadratic_relations = MakeRelations(quadratic_system_size, masters, slaves)

linear_relations = numpy.zeros(unconstrained_linear_stiffness.shape)
for i_linear in range(len(linear_basis)):
    for j_linear in range(len(linear_basis)):
        linear_relations[i_linear::len(linear_basis),j_linear::len(linear_basis)] = quadratic_relations[i_linear::len(quadratic_basis),j_linear::len(quadratic_basis)]


# Compute the constrained stiffnesses
constrained_linear_stiffness = linear_relations.transpose().dot(unconstrained_linear_stiffness).dot(linear_relations).astype(float)
constrained_quadratic_stiffness = quadratic_relations.transpose().dot(unconstrained_quadratic_stiffness).dot(quadratic_relations).astype(float)

# Test restriction operators
#unconstrained_restriction_operator = numpy.array([
#    [1.0, 0.0, 0.0, 0.0, 0.5, 0.0],
#    [0.0, 1.0, 0.0, 0.0, 0.5, 0.0],
#    [0.0, 0.0, 1.0, 0.0, 0.0, 0.5],
#    [0.0, 0.0, 0.0, 1.0, 0.0, 0.5]
#])
#restriction_operator = linear_relations.transpose().dot(unconstrained_restriction_operator).dot(quadratic_relations)

unconstrained_restriction_operator = numpy.array([
    [1.0, 0.0, 0.5, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.5, 0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0, 1.0, 0.0, 0.5],
    [0.0, 0.0, 0.0, 0.0, 1.0, 0.5]
])
constrained_restriction_operator = numpy.array([
    [1.0, 0.0, 0.5, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.5, 1.0, 0.0, 0.5],
    [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0, 0.0, 1.0, 0.5]
])

constrained_interpolation_operator = constrained_restriction_operator.transpose()
restricted_stiffness = constrained_restriction_operator.dot(constrained_quadratic_stiffness).dot(constrained_interpolation_operator)

print(f"quadratic relations:\n{quadratic_relations}\n")
print(f"linear relations:\n{linear_relations}\n")
print(f"constrained linear stiffness:\n{constrained_linear_stiffness}\n")
print(f"constrained quadratic stiffness:\n{constrained_quadratic_stiffness}\n")
print(f"restriction operator:\n{constrained_restriction_operator}\n")
print(f"restricted stiffness:\n{restricted_stiffness}\n")
print(f"restriction error: {numpy.linalg.norm(restricted_stiffness - constrained_linear_stiffness)}")

#for i in range(quadratic_system_size):
#    for j in range(quadratic_system_size):
#        if constrained_quadratic_stiffness[i, j]:
#            print(f"{i}, {j}, {constrained_quadratic_stiffness[i, j]}")
#scipy.io.mmwrite("constrained_quadratic_stiffness", constrained_quadratic_stiffness)

#interpolated_stiffness = interpolation_operator.dot(restricted_stiffness).dot(restriction_operator)
#print(interpolated_stiffness)
#scipy.io.mmwrite("interpolated_stiffness.mm", scipy.sparse.csr_matrix(interpolated_stiffness))
