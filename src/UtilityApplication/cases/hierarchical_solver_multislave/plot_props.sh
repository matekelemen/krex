#!/bin/bash

# Containing directory of this script
script_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$script_dir"

getprops() {
    python -c "
import scipy
import numpy
import pathlib
import argparse
parser = argparse.ArgumentParser()
parser.add_argument('file_path',
                    type = pathlib.Path)
arguments = parser.parse_args()

matrix = scipy.io.mmread(arguments.file_path).todense()
eigenvalues = numpy.linalg.eigvals(matrix)
condition_number = numpy.linalg.cond(matrix)
determinant = numpy.linalg.det(matrix)
spectral_radius = eigenvalues[0] / eigenvalues[-1]
is_symmetric = scipy.linalg.issymmetric(matrix, atol=1e-14, rtol=1e-14)

print(f'{arguments.file_path.stem}:')
print(f'\tcondition number: {condition_number}')
print(f'\tdeterminant: {determinant}')
print(f'\tspectral radius: {spectral_radius}')
print('symmetric' if is_symmetric else 'not_symmtric')
print('\tpositive definite' if all(0 < v for v in eigenvalues) else
        '\tnegative definite' if all(v < 0 for v in eigenvalues) else
        '\tpositive indefinite' if all(0 <= v for v in eigenvalues) else
        '\tnegative indefinite' if all(v <= 0 for v in eigenvalues) else
        '')
" $1
}

if [ -f props ]; then rm props; fi
touch props

for exponent in -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 0 1 2 3 4 5 6 7 8 9 10; do
    mpc_coefficient=$(python -c "import sys; print(2 ** float(sys.argv[1]))" $exponent)
    python MainKratos.py --mesh consistent_quadratic --mpc-coefficient $mpc_coefficient
    echo -n "$mpc_coefficient " >> props
    getprops "system_matrix.mm" | head -4 | tail -3 | cut -f2 -d":" | tr -d "\n" | cut -c2- >> props
done

python -c "
from matplotlib import pyplot
mpc_coefficients = []
condition_numbers = []
determinants = []
spectral_radii = []

with open('props', 'r') as file:
    for line in file.readlines():
        if line:
            for value, container in zip(line.split(' '), [mpc_coefficients, condition_numbers, determinants, spectral_radii]):
                container.append(float(value))

pyplot.plot(mpc_coefficients, condition_numbers, '.')
pyplot.plot(mpc_coefficients, spectral_radii, '+')
pyplot.legend(['condition number', 'spectral radius'])
pyplot.xlabel('mpc coefficient')
pyplot.gca().set_xscale('log')
pyplot.gca().set_yscale('log')
pyplot.gcf().savefig('matrixprops.png')
pyplot.show()
"
