#!/bin/zsh

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

for mesh in "linear" "quadratic" "consistent_linear" "consistent_quadratic"; do
    for solver in "hierarchical_solver" "standalone_amgcl_raw_solver"; do
        # Clean the current directory of old output
        for pattern in "*.mm" "*.png" "*.h5" "*.journal" "*.xdmf"; do
            find . -maxdepth 0 -name $pattern -type f -delete
        done

        # Create new directory for results
        directory_name="${mesh}_mesh_${solver}"

        mkdir -p "$directory_name"
        find "$directory_name" -maxdepth 0 -type f -delete

        # Run the case
        if ! unbuffer python MainKratos.py                          \
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

python filter.py
python diff.py

for directory in "quadratic_mesh_hierarchical_solver" "consistent_quadratic_mesh_hierarchical_solver"; do
    mtx2img $directory/diff.mtx $directory/diff.png -a sum -c kindlmann
done

for directory in "linear_mesh_standalone_amgcl_raw_solver" "consistent_linear_mesh_standalone_amgcl_raw_solver"; do
    mtx2img $directory/system_matrix_1_filtered.mtx $directory/system_matrix_1_filtered.png -a sum -c kindlmann
done
