#!/bin/zsh

script_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$script_dir"

export OMP_NUM_THREADS=1

for mesh in "linear" "quadratic"; do
    for solver in "hierarchical_solver" "standalone_amgcl_raw_solver"; do
        # Clean the current directory of old output
        for pattern in "*.mm" "*.png" "*.h5" "*.journal" "*.xdmf"; do
            find . -maxdepth 0 -name $pattern -type f -delete
        done

        # Create new directory for results
        directory_name="${mesh}_mesh_${solver}"
        mkdir -p "$directory_name"

        # Run the case
        if ! unbuffer python MainKratos.py --mesh $mesh --solver $solver | tee "$directory_name/log"; then
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
