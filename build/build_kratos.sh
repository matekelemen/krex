#!/bin/bash

script_name="$(basename ${BASH_SOURCE[0]})"

print_help() {
    echo "$script_name - Configure, build, and install KratosMultiphysics."
    echo "Usage: $script_name [OPTIONS [ARGUMENT]]"
    echo "-h                    : print this help and exit"
    echo "-C                    : clean build and install directories, then exit"
    echo "-b build_path         : path to the build directory (created if it does not exist yet)"
    echo "-i install_path       : path to the install directory (created if it does not exist yet)"
    echo "-t build_type         : build type [FullDebug, Debug, Release, RelWithDebInfo] (Default: FullDebug)"
    echo "-c compiler_name      : compiler family [gcc, clang, icc] (Default: gcc)"
    echo "-o option             : options/arguments to pass on to CMake. Semicolon (;) delimited, or defined repeatedly."
    echo "-a application_name   : name or path of the application to build (can be passed repeatedly to add more applications)"
    echo
    echo "By default, Kratos is installed to the site-packages directory of the available python"
    echo "interpreter. This makes KratosMultiphysics and its applications immediately available from"
    echo "anywhere on the system without having to append PYTHONPATH, provided that the same interpreter"
    echo "is used. Note however, that it is recommend to use a virtual python environment to avoid tainting"
    echo "the system python."
    echo
    echo "Recommended build tools:"
    echo " - ccache"
    echo " - ninja"
    exit 0
}

if ! command -v python3 &>/dev/null; then
    echo "Error: $script_name requires python3, but could not find it on the system"
    exit 1
fi

get_site_packages_dir() {
    echo $(python3 -c 'import sysconfig; print(sysconfig.get_paths()["purelib"])')
}

# Utility variables
# Path to the directory containing this script
# (assumed to be kratos_repo_root/scripts)
script_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

krex_source_dir="$(dirname "${script_dir}")"    # <== path to the krex repo root
source_dir="${krex_source_dir}/src/kratos"      # <== path to the kratos repo root
app_dir="${source_dir}/applications"            # <== path to the app directory of the kratos repo

generator="Unix Makefiles"                      # <== name of the generator program in CMake
ccache_flag=""                                  # <== sets CXX_COMPILER_LAUNCHER in CMake to ccache if available
mpi_flag="-DUSE_MPI:BOOL=OFF"                   # <== MPI flag to pass to CMake via USE_MPI
mkl_flag="-DUSE_EIGEN_MKL:BOOL=OFF"             # <== toggle support for Intel MKL

# Define default arguments
build_type="Release".                           # <== passed to CMAKE_BUILD_TYPE
build_dir="${source_dir}/build"                 # <== path to the build directory
install_dir="$(get_site_packages_dir)"          # <== path to install kratos to
clean=0                                         # <== clean the build and install directories, then exit
cmake_arguments=""                              # <== additional arguments passed on to CMake

# Function to append the list of applications to build
add_app() {
    if [ "$1" != "${1#/}" ]; then       # <== absolute path
        if [ -d "$1" ]; then
            export KRATOS_APPLICATIONS="${KRATOS_APPLICATIONS}$1;"
        fi
    elif [ -d "$PWD/$1" ]; then         # <== relative path
        export KRATOS_APPLICATIONS="${KRATOS_APPLICATIONS}$PWD/$1;"
    elif [ -d "${app_dir}/$1" ]; then   # <== kratos app name
        export KRATOS_APPLICATIONS="${KRATOS_APPLICATIONS}${app_dir}/$1;"
    else
        echo "Error: cannot find application: $1"
        exit 1
    fi
}

# Parse CL arguments
while getopts ":h C b: i: t: c: o: a:" arg; do
    case $arg in
        h)  # Print help and exit without doing anything.
            print_help
            exit 0
            ;;
        C)  # Set clean flag
            clean=1
            ;;
        b)  # Set build directory
            build_dir="$OPTARG"
            ;;
        i)  # Set install directory
            install_dir="$OPTARG"
            ;;
        t)  # Select build type
            build_type="${OPTARG}"
            if ! [[ "${build_type}" == "FullDebug"           \
                     || "${build_type}" == "Debug"           \
                     || "${build_type}" == "RelWithDebInfo"  \
                     || "${build_type}" == "Release" ]]; then
                echo "Error: invalid build type: ${build_type}"
                print_help
                exit 1
            fi
            ;;
        c)  # Select compilers
            compiler_family="${OPTARG}"
            if [ "$compiler_family" = "gcc" ]; then
                export CC="$(which gcc)"
                export CXX="$(which g++)"
            elif [ "$compiler_family" = "clang" ]; then
                export CC="$(which clang)"
                export CXX="$(which clang++)"
            elif [ "$compiler_family" = "icc" ]; then
                export CC="$(which icc)"
                export CXX="$(which i++)"
            else
                echo "Error: unsupported compiler family: $compiler_family"
                exit 1
            fi
            ;;
        o)  # Append CMake arguments
            if [ "$cmake_arguments" = "" ]; then
                cmake_arguments="$OPTARG"
            else
                cmake_arguments="$cmake_arguments;$OPTARG"
            fi
            ;;
        a)  # Set path to a file containing a list of applications' names to be compiled
            add_app "${OPTARG}"
            ;;
        \?) # Invalid argument
            echo "Error: unrecognized argument: ${OPTARG}"
            print_help
            exit 1
    esac
done

# Check write access to the build directory
if [ -d "$build_dir" ]; then
    if ! [[ -w "$build_dir" ]]; then
        echo "Error: user '$(hostname)' has no write access to the build directory: '$build_dir'"
        exit 1
    fi
fi

# Check write access to the install dir
if [ -d "$install_dir" ]; then
    if ! [[ -w "$install_dir" ]]; then
        echo "Error: user '$(hostname)' has no write access to the install directory: '$install_dir'"
        exit 1
    fi
fi

# If requested, clear build and install directories, then exit
if [ $clean -ne 0 ]; then
    for item in "$build_dir"; do
        rm -rf "$item"
    done
    for item in "$install_dir/KratosMultiphysics"; do
        rm -rf "$item"
    done
    for item in "$install_dir/libs"; do
        rm -rf "$item"
    done
    exit 0
fi

# Check whether intel mkl is available
if [ -f "/opt/intel/oneapi/setvars.sh" ] && [ "$compiler_family" != "clang" ]; then
    source "/opt/intel/oneapi/setvars.sh"
    mkl_flag="-DUSE_EIGEN_MKL:BOOl=ON"
fi

# Check whether MPI is available
if command -v mpirun $> /dev/null; then
    mpi_flag="-DUSE_MPI:BOOL=ON"
fi

# Check optional dependency - ninja
if command -v ninja &>/dev/null; then
    generator="Ninja"
fi

# Check optional dependency - ccache
if command -v ccache &>/dev/null; then
    ccache_flag="-DCMAKE_CXX_COMPILER_LAUNCHER:STRING=ccache"
fi

# Clear CMake cache
rm -f "$build_dir/CMakeCache.txt"

# Generate with CMake
export KRATOS_INSTALL_PYTHON_USING_LINKS="ON"
if ! cmake                                                  \
    "-H$source_dir"                                         \
    "-B$build_dir"                                          \
    "-DCMAKE_INSTALL_PREFIX:STRING=$install_dir"            \
    "-G${generator}"                                        \
    "-DCMAKE_BUILD_TYPE:STRING=$build_type"                 \
    "-DCMAKE_COLOR_DIAGNOSTICS:BOOL=ON"                     \
    "$ccache_flag"                                          \
    "$mpi_flag"                                             \
    "$mkl_flag"                                             \
    "-DKRATOS_GENERATE_PYTHON_STUBS:BOOL=ON"                \
    $(echo $cmake_arguments | tr '\;' '\n')                 \
    ; then
    exit 1
fi

physical_cores=$(grep "^cpu\\scores" /proc/cpuinfo | uniq |  awk '{print $4}')

# Build and install
if ! cmake --build "$build_dir" --config "$build_type" --target install -j $physical_cores; then
    exit 1
fi
