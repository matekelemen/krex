#!/bin/zsh

set -e

# Containing directory of this script
script_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$script_dir"

print_help() {
    echo "run.sh [-c mpc_coefficient] [-o mpc_offset]"
}

while getopts ":h" arg; do
    case "$arg" in
        h)  # Print help and exit
            print_help
            exit 0
            ;;
        \?) # Unrecognized arment
            echo "Error: unrecognized argument: -$OPTARG"
            print_help
            exit 1
    esac
done

for mesh in "x1" "x2" "x4" "linear_x1" "linear_x2" "linear_x4"; do
    for solver in "hierarchical_solver" "standalone_amgcl_raw_solver"; do
        for constrained_group in "top_10" "top_40" "top_90" "volume_10"; do
            # Clean the current directory of old output
            find "$(pwd)" -maxdepth 1 \( -name "*.mm" -o -name "*.png" -o -name "*.h5" -o -name "*.journal" -o -name "*.xdmf" \) -delete

            # Create new directory for results
            directory_name="${mesh}_mesh_${solver}_${constrained_group}"
            mkdir -p "$directory_name"
            find "$directory_name" -maxdepth 1 -type f -delete

            # Run the case
            if ! unbuffer python main.py                                        \
                                --mesh $mesh                                    \
                                --solver $solver                                \
                                --constrained-group "root_$constrained_group"   \
                    | tee "$directory_name/log"; then
                exit 1
            fi

            for f in *.h5; do mv $f $directory_name; done

            # Move result output
            cd "$script_dir"
        done # for constrained_group
    done # for solver
done # for mesh

