// --- Core Includes ---
#include "utilities/sparse_matrix_multiplication_utility.h"
#include "spaces/ublas_space.h"

// --- STL Includes ---
#include <filesystem>
#include <sstream>
#include <chrono>
#include <ctime>


namespace Kratos {


using Space = TUblasSparseSpace<double>;


void read(const std::filesystem::path& rPath,
          Space::MatrixType& rTargetMatrix)
{
    std::ifstream file(rPath);
    std::string line;

    // Expect exactly 1 comment line ==> ignore it
    std::getline(file, line);

    // Expect size line ==> parse it
    std::size_t row_count=0ul, column_count=0ul, nonzero_count=0ul;
    std::getline(file, line);
    {
        std::istringstream stream(line);
        if (!(stream >> row_count >> column_count >> nonzero_count)) throw 1;
    }

    rTargetMatrix.resize(row_count, column_count, /*preserve=*/false);

    while (std::getline(file, line)) {
        // Ignore empty lines
        if (line.empty()) continue;

        std::istringstream stream(line);
        std::size_t i_row = 0ul, i_column = 0ul;
        double value;
        if (!(stream >> i_row >> i_column >> value)) throw 1;

        // Matrix market works with 1-based indices
        --i_row;
        --i_column;

        rTargetMatrix.insert_element(i_row, i_column, value);
    }

    rTargetMatrix.complete_index1_data();
}


int main()
{
    Space::MatrixType left, right;

    try {read("left.mm", left);} catch (...) {return 1;}
    try {read("right.mm", right);} catch (...) {return 2;}

    Space::MatrixType result;

    std::cout << "Begin matrix product\n";
    const auto begin = std::chrono::system_clock::now();
    SparseMatrixMultiplicationUtility::MatrixMultiplication(left, right, result);
    KRATOS_INFO("Matrix product duration")
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin).count() << " ms"
        << (result.nnz() == 0 ? " " : "\n"); // <== dummy line to use the result

    return 0;
}


} // namespace Kratos


int main(int argc, const char** const argv)
{
    return Kratos::main();
}
