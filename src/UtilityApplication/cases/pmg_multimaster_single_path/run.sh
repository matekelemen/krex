#!/bin/bash

set -e

script_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$script_dir"

# Default arguments
mpc_coefficient=1e0
mpc_constant=0e0

print_help() {
    echo "run.sh [-c mpc_coefficient] [-o mpc_offset]"
}

while getopts ":h c: o:" arg; do
    case "$arg" in
        h)  # Print help and exit
            print_help
            exit 0
            ;;
        c)  # Set MPC coefficient
            mpc_coefficient="$OPTARG"
            ;;
        o)  # Set MPC constant
            mpc_constant="$OPTARG"
            ;;
        \?) # Unrecognized arment
            echo "Error: unrecognized argument: -$OPTARG"
            print_help
            exit 1
    esac
done

export OMP_NUM_THREADS=1

for mesh in "linear" "quadratic"; do
    for solver in "hierarchical_solver" "standalone_amgcl_raw_solver"; do
        # Clean the current directory of old output
        find "$(pwd)" -maxdepth 1 \( -name "*.mm" -o -name "*.png" -o -name "*.h5" -o -name "*.journal" -o -name "*.xdmf" \) -delete

        # Create new directory for results
        directory_name="${mesh}_mesh_${solver}"
        rm -rf "$directory_name"
        mkdir -p "$directory_name"

        # Run the case
        if ! unbuffer python main.py                                \
                             --mesh $mesh                           \
                             --solver $solver                       \
                             --mpc-coefficient "$mpc_coefficient"   \
                             --mpc-constant "$mpc_constant"         \
                | tee "$directory_name/log"; then
            exit 1
        fi

        # Move result output
        mv *.h5 "$directory_name/"

        # Generate XDMF
        if python -c "import KratosMultiphysics; import KratosMultiphysics.WRApplication"; then
            if ! python -m KratosMultiphysics.WRApplication         \
                GenerateHDF5Journal                                 \
                --file-name $directory_name/structure_\<time\>.h5   \
                --journal-name $directory_name/structure.journal
                then
                exit 1
            fi

            if ! python -m KratosMultiphysics.WRApplication         \
                GenerateXDMF                                        \
                --journal-path $directory_name/structure.journal    \
                --output-pattern $directory_name/structure.xdmf
                then
                exit 1
            fi
        fi # if WRApplication is available

        # Move output from hierarchical solver
        mv *.mm $directory_name/

        # Generate images from matrices
        if command -v mtx2img &> /dev/null; then
            for f in $directory_name/*.mm; do
                if ! mtx2img "$f" "${f%.mm}.png" -a sum -c kindlmann; then
                    exit 1
                fi
            done # for f
        fi # if mtx2img
    done # for solver
done # for mesh

python << EOF
import scipy
import numpy
linear = scipy.io.mmread('linear_mesh_standalone_amgcl_raw_solver/system_matrix_1.mm').todense()
coarse = scipy.io.mmread('quadratic_mesh_hierarchical_solver/coarse_system_matrix.mm').todense()
constrained_dofs = [0, 1, 2, 3, 4, 5, 6, 7, 10, 11]
linear = numpy.delete(linear, constrained_dofs, axis = 0)
linear = numpy.delete(linear, constrained_dofs, axis = 1)
diff = linear - coarse
scipy.io.mmwrite('diff', diff, precision=13, symmetry='general')
diff_norm = numpy.linalg.norm(diff)
print(f'diff norm: {diff_norm}')
if 1e-14 < diff_norm:
    exit(1)
else:
    exit(0)
EOF

mtx2img diff.mtx diff.png -a sum -c kindlmann
