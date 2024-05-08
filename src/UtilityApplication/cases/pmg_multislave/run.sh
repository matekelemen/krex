#!/bin/zsh

set -e

# Containing directory of this script
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

for mesh in "linear" "quadratic" "consistent_linear" "consistent_quadratic"; do
    for solver in "hierarchical_solver" "standalone_amgcl_raw_solver"; do
        # Clean the current directory of old output
        find "$(pwd)" -maxdepth 1 \( -name "*.mm" -o -name "*.png" -o -name "*.h5" -o -name "*.journal" -o -name "*.xdmf" \) -delete

        # Create new directory for results
        directory_name="${mesh}_mesh_${solver}"
        mkdir -p "$directory_name"
        find "$directory_name" -maxdepth 1 -type f -delete

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
        cd "$directory_name"
        if python -c "import KratosMultiphysics; import KratosMultiphysics.WRApplication"; then
            if ! python -m KratosMultiphysics.WRApplication         \
                GenerateHDF5Journal                                 \
                --file-name structure_\<time\>.h5   \
                --journal-name structure.journal
                then
                exit 1
            fi

            if ! python -m KratosMultiphysics.WRApplication         \
                GenerateXDMF                                        \
                --journal-path structure.journal    \
                --output-pattern structure.xdmf
                then
                exit 1
            fi
        fi # if WRApplication is available
        cd "$script_dir"

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

#python filter.py
#python diff.py
#
#for directory in "quadratic_mesh_hierarchical_solver" "consistent_quadratic_mesh_hierarchical_solver"; do
#    mtx2img $directory/diff.mtx $directory/diff.png -a sum -c kindlmann
#done
#
#for directory in "linear_mesh_standalone_amgcl_raw_solver" "consistent_linear_mesh_standalone_amgcl_raw_solver"; do
#    mtx2img $directory/system_matrix_1_filtered.mtx $directory/system_matrix_1_filtered.png -a sum -c kindlmann
#done

python << EOF
import scipy
import numpy
linear = scipy.io.mmread('linear_mesh_standalone_amgcl_raw_solver/system_matrix_1.mm').todense()
coarse = scipy.io.mmread('quadratic_mesh_hierarchical_solver/coarse_system_matrix.mm').todense()
constrained_dofs = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 16, 17]
linear = numpy.delete(linear, constrained_dofs, axis = 0)
linear = numpy.delete(linear, constrained_dofs, axis = 1)
diff = linear - coarse
scipy.io.mmwrite('diff', scipy.sparse.csr_matrix(diff), precision=13, symmetry='general')
diff_norm = numpy.linalg.norm(diff)
print(f'diff norm: {diff_norm}')
EOF

mtx2img diff.mtx quadratic_mesh_hierarchical_solver/diff.png -a sum -c kindlmann
mv diff.mtx quadratic_mesh_hierarchical_solver/diff.mm

python << EOF
import scipy
import numpy
linear = scipy.io.mmread('consistent_linear_mesh_standalone_amgcl_raw_solver/system_matrix_1.mm').todense()
coarse = scipy.io.mmread('consistent_quadratic_mesh_hierarchical_solver/coarse_system_matrix.mm').todense()
constrained_dofs = [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]
linear = numpy.delete(linear, constrained_dofs, axis = 0)
linear = numpy.delete(linear, constrained_dofs, axis = 1)
diff = linear - coarse
scipy.io.mmwrite('diff', scipy.sparse.csr_matrix(diff), precision=13, symmetry='general')
diff_norm = numpy.linalg.norm(diff)
print(f'consistent diff norm: {diff_norm}')
EOF

mtx2img diff.mtx consistent_quadratic_mesh_hierarchical_solver/diff.png -a sum -c kindlmann
mv diff.mtx consistent_quadratic_mesh_hierarchical_solver/diff.mm
