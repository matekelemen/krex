//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//
//  Main authors:    Máté Kelemen
//

#ifndef AMGCL_PARAM_UNKNOWN
    #define AMGCL_PARAM_UNKNOWN(NAME)                       \
        KRATOS_ERROR                                        \
            << KRATOS_CODE_LOCATION                         \
            << "Unknown parameter " << NAME << std::endl
#endif

// Project includes
#include "UtilityApp/AMGCLWrapper.hpp"
#include "spaces/ublas_space.h"
#include "utilities/profiler.h"
#include "input_output/logger.h"

// External includes
#include "amgcl/adapter/ublas.hpp"
#include "amgcl/adapter/zero_copy.hpp"
#include "amgcl/backend/builtin.hpp"
#include "amgcl/value_type/static_matrix.hpp"
#include "amgcl/make_solver.hpp"
#include "amgcl/make_block_solver.hpp"
#include "amgcl/solver/runtime.hpp"
#include "amgcl/preconditioner/runtime.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"

#ifdef AMGCL_GPGPU
#include <amgcl/backend/vexcl.hpp>
#include <amgcl/backend/vexcl_static_matrix.hpp>
#endif

// STL includes
#include <sstream> // stringstream
#include <variant> // variant


namespace Kratos {


#ifdef AMGCL_GPGPU
vex::Context& GetVexCLContext() {
    [[maybe_unused]] static bool run_once = [](){
        return true;
    }();

    // Pick a strategy for device selection. If any
    // OCL_* environment variables are defined, use
    // the environment filter, otherwise fall back to
    // GPU.

    for (const auto& ocl_variable : {std::string("OCL_PLATFORM"),
                                     std::string("OCL_VENDOR"),
                                     std::string("OCL_DEVICE"),
                                     std::string("OCL_TYPE"),
                                     std::string("OCL_MAX_DEVICES"),
                                     std::string("OCL_POSITION")}) {
        if (std::getenv(ocl_variable.c_str())) {
            static vex::Context ctx(vex::Filter::Env);
            return ctx;
        }
    }

    static vex::Context ctx(vex::Filter::GPU);
    return ctx;
}

template <class T, int TBlockSize>
void RegisterVexCLStaticMatrixType() {
    static vex::scoped_program_header header(GetVexCLContext(),
            amgcl::backend::vexcl_static_matrix_declaration<T,TBlockSize>());
}
#endif


// -------------------------------------
// --- Template Nightmare Land Begin ---
// -------------------------------------


namespace Detail {


template <class TValue, unsigned TBlockSize>
using AMGCLBlock = amgcl::static_matrix<TValue,TBlockSize,TBlockSize>;


// Trait class defining an AMGCL backend for the provided template arguments,
// and its associated preconditioner, wrapper, and solver.
template <class TSparseSpace, unsigned TBlockSize>
struct AMGCLTraits
{
    template <template <class, class ...> class TBackend, class TValue, class ...TArgs>
    struct Impl
    {
        using SparseSpace = TSparseSpace;

        constexpr static inline unsigned BlockSize = TBlockSize;

        // Indicates whether the system matrix and vectors must be transferred
        // to/from another device (GPU) before/after invoking the solver.
        constexpr static inline bool IsGPUBound =
        #ifdef AMGCL_GPGPU
            std::is_same_v<TBackend<TValue,TArgs...>,amgcl::backend::vexcl<TValue,TArgs...>>;
        #else
            false;
        #endif

        constexpr static inline bool IsScalar = TBlockSize == 1;

        using Scalar = TValue;

        using Value = std::conditional_t<
            IsScalar,
            Scalar,
            amgcl::static_matrix<Scalar,TBlockSize,TBlockSize>
        >;

        using RHS = std::conditional_t<
            IsScalar,
            Scalar,
            amgcl::static_matrix<TValue,TBlockSize,1>
        >;

        using MatrixAdapter = std::conditional_t<
            IsScalar,
            amgcl::backend::crs<Scalar>,
            amgcl::adapter::block_matrix_adapter<
                std::tuple<std::size_t,
                           amgcl::iterator_range<const std::size_t*>,
                           amgcl::iterator_range<const std::size_t*>,
                           amgcl::iterator_range<const Scalar*>>,
                Value
            >
        >;

        using Backend = std::conditional_t<
            IsScalar,
            TBackend<TValue>,
            TBackend<AMGCLBlock<TValue,TBlockSize>>
        >;

        using Preconditioner = amgcl::runtime::preconditioner<Backend>;

        using SolverWrapper = amgcl::runtime::solver::wrapper<Backend>;

        using Solver = std::conditional_t<
            IsScalar,
            amgcl::make_solver<Preconditioner,SolverWrapper>,
            amgcl::make_block_solver<Preconditioner,SolverWrapper>
        >;
    }; // struct Impl
}; // struct AMGCLTraits


// Wrapper class bundling a solver with an associated matrix adapter.
template <class TAMGCLTraits>
struct AMGCLBundle
{
    using Traits = TAMGCLTraits;

    std::unique_ptr<typename Traits::Solver> mpSolver;

    std::shared_ptr<typename Traits::MatrixAdapter> mpMatrixAdapter;

    /// @brief Construct an AMGCL solver and its supporting variables.
    /// @details The constructor has 3 main jobs to take care of:
    ///          - copy the system matrix if the backend's and sparse space's
    ///            scalar types differ.
    ///          - construct a matrix adapter for AMGCL that provides a view on
    ///            the system matrix or (its copy).
    ///          - construct the AMGCL solver using the matrix adapter.
    ///          The AMGCL backend type depends on the @ref AMGCLTraits::Impl
    ///          type that this class is instantiated with (@a TAMGCLTraits).
    ///          - CPU or GPU backend
    ///          - single or double precision scalar type
    ///          - block size
    AMGCLBundle(const typename TAMGCLTraits::SparseSpace::MatrixType& rSystemMatrix,
                const boost::property_tree::ptree& rSolverSettings)
        : mpSolver(),
          mpMatrixAdapter()
    {
        KRATOS_TRY

        // Copy the system matrix if necessary and construct the adapter.
        mpMatrixAdapter = std::make_shared<typename TAMGCLTraits::MatrixAdapter>(
            amgcl::backend::map(rSystemMatrix));

        // Construct solver
        if constexpr (TAMGCLTraits::IsGPUBound) {
            #ifdef AMGCL_GPGPU
                typename TAMGCLTraits::Backend::params backend_parameters;
                backend_parameters.q = GetVexCLContext();
                mpSolver = std::make_unique<typename TAMGCLTraits::Solver>(*mpMatrixAdapter,
                                                                           rSolverSettings,
                                                                           backend_parameters);
            #else
                KRATOS_ERROR << "internal solver error: requesting a GPU solver while Kratos is compiled without GPU support\n";
            #endif
        } else {
            mpSolver = std::make_unique<typename TAMGCLTraits::Solver>(*mpMatrixAdapter,
                                                                       rSolverSettings);
        }

        KRATOS_CATCH("")
    }
}; // struct AMGCLBundle


} // namespace Detail


// -------------------------------------
// --- Template Nightmare Land End -----
// -------------------------------------


enum class AMGCLBackendType
{
    CPU
    #ifdef AMGCL_GPGPU
    ,GPU
    #endif
}; // enum class VexCLBackendType


template <class TSparseSpace,
          class TDenseSpace,
          class TReorderer>
struct AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::Impl
{
    using ValueType = typename TSparseSpace::DataType;

    int mVerbosity;

    double mTolerance;

    // AMGCL backend selected by the user.
    AMGCLBackendType mBackendType = AMGCLBackendType::CPU;

    // Block size computed in AMGCLWrapper::ProvideAdditionalData and
    // set in the settings passed to AMGCL.
    std::optional<std::size_t> mMaybeDoFCount;

    // Settings to be translated from Kratos::Parameters
    // to something that AMGCL uses.
    boost::property_tree::ptree mAMGCLSettings;

    // Pointer to the system matrix to be set in AMGCLWrapper::ProvideAdditionalData.
    // This matrix is then required to reside at the same address when invoking
    // AMGCLWrapper::Solve.
    const typename TSparseSpace::MatrixType* mpA = nullptr;

    template <unsigned BlockSize>
    using AMGCLTraits = Detail::AMGCLTraits<TSparseSpace,BlockSize>;

    // A variant for grouping members related to the wrapped AMGCL solver. It's meant
    // to bundle solvers using different backends as well as its associated matrix wrapper.
    // The wrapped types are composed of the permutations of the following attributes:
    // - block size [1, 2, 3, 4, 5, 6]
    // - backend type [bultin, vexcl]
    std::variant<
        // Dummy type to enable the default constructor.
        std::monostate,

        // Double precision CPU backend with a block size of 1.
        Detail::AMGCLBundle<typename AMGCLTraits<1>::template Impl<amgcl::backend::builtin,ValueType>>,

        // Double precision CPU backend with a block size of 2.
        Detail::AMGCLBundle<typename AMGCLTraits<2>::template Impl<amgcl::backend::builtin,ValueType>>,

        // Double precision CPU backend with a block size of 3.
        Detail::AMGCLBundle<typename AMGCLTraits<3>::template Impl<amgcl::backend::builtin,ValueType>>,

        // Double precision CPU backend with a block size of 4.
        Detail::AMGCLBundle<typename AMGCLTraits<4>::template Impl<amgcl::backend::builtin,ValueType>>,

        // Double precision CPU backend with a block size of 5.
        Detail::AMGCLBundle<typename AMGCLTraits<5>::template Impl<amgcl::backend::builtin,ValueType>>,

        // Double precision CPU backend with a block size of 6.
        Detail::AMGCLBundle<typename AMGCLTraits<6>::template Impl<amgcl::backend::builtin,ValueType>>

        #ifdef AMGCL_GPGPU

        // Double precision GPU backend with a block size of 1.
        ,Detail::AMGCLBundle<typename AMGCLTraits<1>::template Impl<amgcl::backend::vexcl,ValueType>>,

        // Double precision GPU backend with a block size of 2.
        Detail::AMGCLBundle<typename AMGCLTraits<2>::template Impl<amgcl::backend::vexcl,ValueType>>,

        // Double precision GPU backend with a block size of 3.
        Detail::AMGCLBundle<typename AMGCLTraits<3>::template Impl<amgcl::backend::vexcl,ValueType>>,

        // Double precision GPU backend with a block size of 4.
        Detail::AMGCLBundle<typename AMGCLTraits<4>::template Impl<amgcl::backend::vexcl,ValueType>>,

        // Double precision GPU backend with a block size of 5.
        Detail::AMGCLBundle<typename AMGCLTraits<5>::template Impl<amgcl::backend::vexcl,ValueType>>,

        // Double precision GPU backend with a block size of 6.
        Detail::AMGCLBundle<typename AMGCLTraits<6>::template Impl<amgcl::backend::vexcl,ValueType>>

        #endif
    > mSolverBundle;
}; // struct AMGCLWrapper::Impl



template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::AMGCLWrapper(Parameters parameters)
    : mpImpl(new Impl)
{
    KRATOS_TRY
    Parameters default_parameters = this->GetDefaultParameters();
    if (parameters.Has("block_size")) {
        if (parameters["block_size"].Is<int>()) {
            default_parameters.RemoveValue("block_size");
            default_parameters.AddInt("block_size", 1);
            mpImpl->mMaybeDoFCount = parameters["block_size"].Get<int>();
        } else if (parameters["block_size"].Is<std::string>()) {
            const std::string block_size_string = parameters["block_size"].Get<std::string>();
            KRATOS_ERROR_IF_NOT(block_size_string == "auto")
                << "\"block_size\" must either be \"auto\" or a positive integer, not "
                << "\"" << block_size_string << "\"";
        }
    }

    parameters.ValidateAndAssignDefaults(default_parameters);

    KRATOS_ERROR_IF_NOT(parameters["solver_type"].GetString() == "amgcl_wrapper" or parameters["solver_type"].GetString() == "UtilityApplication.amgcl_wrapper")
        << "Requested a(n) '" << parameters["solver_type"].GetString() << "' solver,"
        << " but constructing an AMGCLWrapper";

    mpImpl->mVerbosity = parameters["verbosity"].Get<int>();
    mpImpl->mTolerance = parameters["tolerance"].Get<double>();

    // Get requested backend type. Supported arguments:
    // - "cpu"    : built-in cpu backend
    // - "gpu"   : vexcl gpu interface
    const std::string requested_backend_type = parameters["backend"].GetString();
    if (requested_backend_type == "cpu") {
        // CPU backend by default
        mpImpl->mBackendType = AMGCLBackendType::CPU;
    } else if (requested_backend_type == "gpu") {
        #ifdef AMGCL_GPGPU
            RegisterVexCLStaticMatrixType<typename Impl::ValueType, 2>();
            RegisterVexCLStaticMatrixType<typename Impl::ValueType, 3>();
            RegisterVexCLStaticMatrixType<typename Impl::ValueType, 4>();
            RegisterVexCLStaticMatrixType<typename Impl::ValueType, 5>();
            RegisterVexCLStaticMatrixType<typename Impl::ValueType, 6>();
            mpImpl->mBackendType = AMGCLBackendType::GPU;
        #else
            KRATOS_ERROR << "the requested backend 'gpu' is not available because Kratos was compiled without GPU support\n";
        #endif
    } else {
        KRATOS_ERROR << "unsupported argument for 'backend': "
                     << requested_backend_type
                     << ". Available options are \"cpu\""
                     #ifdef AMGCL_GPGPU
                     << " or \"gpu\""
                     #endif
                     << ".\n";
    }

    // Convert parameters to AMGCL settings
    std::stringstream json_stream;
    json_stream << parameters["amgcl_settings"].PrettyPrintJsonString();
    boost::property_tree::read_json(
        json_stream,
        mpImpl->mAMGCLSettings
    );
    KRATOS_CATCH("")
}



// Necessary for PIMPL
template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::~AMGCLWrapper()
{
}



template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
bool AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::PerformSolutionStep(SparseMatrix& rA,
                                                                            Vector& rX,
                                                                            Vector& rB)
{
    KRATOS_TRY

    // Override AMGCL settings
    //mpImpl->mAMGCLSettings.put("solver.verbose", 1 < mpImpl->mVerbosity);

    KRATOS_ERROR_IF_NOT(&rA == mpImpl->mpA)
        << "solver got a different matrix than it was initialized with "
        << &rA << " != " << mpImpl->mpA << "\n";

    const auto [iteration_count, residual] = std::visit(
        [&rB, &rX] (auto& rBundle) -> std::tuple<std::size_t,double> {
            using BundleType = std::remove_reference_t<decltype(rBundle)>;

            if constexpr (!std::is_same_v<BundleType,std::monostate>) {
                const std::size_t system_block_size = rX.size() / BundleType::Traits::BlockSize;

                #ifdef AMGCL_GPGPU
                if constexpr (BundleType::Traits::IsGPUBound) {
                    auto& vexcl_context = GetVexCLContext();
                    KRATOS_ERROR_IF_NOT(vexcl_context) << "invalid VexCL context state\n";

                    typename BundleType::Traits::RHS* x_begin = reinterpret_cast<typename BundleType::Traits::RHS*>(&*rX.begin());
                    typename BundleType::Traits::RHS* b_begin = reinterpret_cast<typename BundleType::Traits::RHS*>(&*rB.begin());

                    // Transfer system vectors to the GPU
                    vex::vector<typename BundleType::Traits::RHS> x_gpu(vexcl_context, system_block_size, x_begin);
                    vex::vector<typename BundleType::Traits::RHS> b_gpu(vexcl_context, system_block_size, b_begin);

                    // Solve
                    const auto results = rBundle.mpSolver->operator()(b_gpu, x_gpu);

                    // Fetch solution from the GPU and return
                    vex::copy(x_gpu.begin(), x_gpu.end(), x_begin);
                    return results;
                } /*if BundleType::Traits::IsGPUBound*/ else
                #endif
                {
                    // Solve and return
                    auto x_begin = reinterpret_cast<typename BundleType::Traits::RHS*>(&*rX.begin());
                    auto b_begin = reinterpret_cast<typename BundleType::Traits::RHS*>(&*rB.begin());
                    return rBundle.mpSolver->operator()(boost::make_iterator_range(b_begin, b_begin + system_block_size),
                                                        boost::make_iterator_range(x_begin, x_begin + system_block_size));
                }
            } else /*BundleType != std::monostate*/ {
                KRATOS_ERROR << "AMGCL solver type is unset. Did you forget to call AMGCLWrapper::ProvideAdditionalData?\n";
            }
        },
        mpImpl->mSolverBundle
    );

    KRATOS_WARNING_IF("AMGCLWrapper", 1 <= mpImpl->mVerbosity && mpImpl->mTolerance <= residual)
        << "Failed to converge. Residual: " << residual << "\n";

    if(2 <= mpImpl->mVerbosity) {
        std::cout << "Iterations: " << iteration_count << "\n"
                  << "Error: " << residual << "\n"
                  << "\n";
    }

    return residual < mpImpl->mTolerance;
    KRATOS_CATCH("")
}


template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
bool AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::Solve(SparseMatrix& rA,
                                                              Vector& rX,
                                                              Vector& rB)
{
    this->InitializeSolutionStep(rA, rX, rB);
    return this->PerformSolutionStep(rA, rX, rB);
}



template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
bool AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::AdditionalPhysicalDataIsNeeded()
{
    return true;
}



template<class TSparseSpace>
std::size_t FindBlockSize(const ModelPart& rModelPart,
                          ModelPart::DofsArrayType& rDofs)
{
    KRATOS_TRY
    int old_ndof    = -1,
        ndof        =  0,
        block_size  =  0;

    if (!rModelPart.IsDistributed())
    {
        unsigned int old_node_id = rDofs.size() ? rDofs.begin()->Id() : 0;
        for (auto it = rDofs.begin(); it!=rDofs.end(); it++) {
            if(it->EquationId() < rDofs.size()) {
                IndexType id = it->Id();
                if(id != old_node_id) {
                    old_node_id = id;
                    if(old_ndof == -1) old_ndof = ndof;
                    else if(old_ndof != ndof) { //if it is different than the block size is 1
                        old_ndof = -1;
                        break;
                    }

                    ndof=1;
                } else {
                    ndof++;
                }
            }
        }

        if(old_ndof == -1 || old_ndof != ndof)
            block_size = 1;
        else
            block_size = ndof;

    }
    else //distribute
    {
        const std::size_t system_size = rDofs.size();
        int current_rank = rModelPart.GetCommunicator().GetDataCommunicator().Rank();
        unsigned int old_node_id = rDofs.size() ? rDofs.begin()->Id() : 0;
        for (auto it = rDofs.begin(); it!=rDofs.end(); it++) {
            if(it->EquationId() < system_size  && it->GetSolutionStepValue(PARTITION_INDEX) == current_rank) {
                IndexType id = it->Id();
                if(id != old_node_id) {
                    old_node_id = id;
                    if(old_ndof == -1) old_ndof = ndof;
                    else if(old_ndof != ndof) { //if it is different than the block size is 1
                        old_ndof = -1;
                        break;
                    }

                    ndof=1;
                } else {
                    ndof++;
                }
            }
        }

        if(old_ndof != -1)
            block_size = ndof;

        int max_block_size = rModelPart.GetCommunicator().GetDataCommunicator().MaxAll(block_size);

        if( old_ndof == -1) {
            block_size = max_block_size;
        }

        KRATOS_ERROR_IF(block_size != max_block_size) << "Block size is not consistent. Local: " << block_size  << " Max: " << max_block_size << std::endl;
    }

    return block_size;
    KRATOS_CATCH("")
}



template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
void AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::InitializeSolutionStep(SparseMatrix& rA,
                                                                               Vector& rX,
                                                                               Vector& rB)
{
    KRATOS_TRY
    KRATOS_PROFILE_SCOPE(KRATOS_CODE_LOCATION);

    if (!mpImpl->mMaybeDoFCount.has_value())
        mpImpl->mMaybeDoFCount = 1;
    KRATOS_INFO_IF("AMGCLWrapper", 2 <= mpImpl->mVerbosity)
        << "block size: " << mpImpl->mMaybeDoFCount.value() << "\n";

    // Construct solver and matrix adapter
    mpImpl->mpA = &rA;

    #define KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, BLOCK_SIZE)  \
        using Bundle = Detail::AMGCLBundle<typename Detail::AMGCLTraits<TSparseSpace,1>::template               \
                    Impl<BACKEND_TEMPLATE,BACKEND_SCALAR>>;                                                     \
        mpImpl->mSolverBundle = Bundle(rA, mpImpl->mAMGCLSettings)

    #define KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE(BACKEND_TEMPLATE, BACKEND_SCALAR)                          \
        switch (mpImpl->mMaybeDoFCount.value()) {                                                           \
            case 1: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 1);  \
                break;                                                                                      \
            }                                                                                               \
            case 2: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 2);  \
                break;                                                                                      \
            }                                                                                               \
            case 3: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 3);  \
                break;                                                                                      \
            }                                                                                               \
            case 4: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 4);  \
                break;                                                                                      \
            }                                                                                               \
            case 5: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 5);  \
                break;                                                                                      \
            }                                                                                               \
            case 6: {                                                                                       \
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE(BACKEND_TEMPLATE, BACKEND_SCALAR, 6);  \
                break;                                                                                      \
            }                                                                                               \
            default: KRATOS_ERROR << "unsupported block size: " << mpImpl->mMaybeDoFCount.value() << "\n";  \
        } // switch mpImpl->mMaybeDoFCount.value()

    // Construct the solver
    switch (mpImpl->mBackendType) {
        #ifdef AMGCL_GPGPU
            case AMGCLBackendType::GPU: {
                KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE(amgcl::backend::vexcl, typename Impl::ValueType);
                KRATOS_INFO_IF("AMGCLWrapper", 2 <= mpImpl->mVerbosity) << GetVexCLContext();
                break;
            }
        #endif
        case AMGCLBackendType::CPU: {
            KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE(amgcl::backend::builtin, typename Impl::ValueType);
            break;
        } // case AMGCLBackendType::CPU
        default: KRATOS_ERROR << "unhandled AMGCL backend enum: " << (int)mpImpl->mBackendType << "\n";
    } // switch mpImpl->mBackendType

    #undef KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE
    #undef KRATOS_CONSTRUCT_AMGCL_SOLVER_BUNDLE_WITH_BLOCK_SIZE
    KRATOS_CATCH("")
}


template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
void AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::ProvideAdditionalData(SparseMatrix& rA,
                                                                              Vector& rX,
                                                                              Vector& rB,
                                                                              ModelPart::DofsArrayType& rDofs,
                                                                              ModelPart& rModelPart)
{
    KRATOS_TRY
    if (!mpImpl->mMaybeDoFCount.has_value())
        mpImpl->mMaybeDoFCount = FindBlockSize<TSparseSpace>(rModelPart, rDofs);
    KRATOS_CATCH("")
}



template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
Parameters
AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::GetDefaultParameters()
{
    return Parameters(R"(
{
    "solver_type" : "amgcl_wrapper",
    "verbosity" : 0,
    "tolerance" : 1e-6,
    "backend" : "cpu",
    "block_size" : "auto",
    "amgcl_settings" : {
        "precond" : {
            "class" : "amg",
            "relax" : {
                "type" : "ilu0"
            },
            "coarsening" : {
                "type" : "aggregation",
                "aggr" : {
                    "eps_strong" : 0.08,
                    "block_size" : 1
                }
            },
            "coarse_enough" : 333,
            "npre" : 1,
            "npost" : 1
        },
        "solver" : {
            "type" : "cg",
            "maxiter" : 555,
            "tol" : 1e-6
        }
    }
}
    )");
}


template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
bool AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::Solve(SparseMatrix& rA,
                                                              DenseMatrix& rX,
                                                              DenseMatrix& rB)
{
    return false;
}


template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
void AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::PrintInfo(std::ostream& rStream) const
{
    rStream << "AMGCLWrapper";
}


template<class TSparseSpace,
         class TDenseSpace,
         class TReorderer>
void AMGCLWrapper<TSparseSpace,TDenseSpace,TReorderer>::PrintData(std::ostream& rStream) const
{
    rStream
        << "tolerance     : " << mpImpl->mTolerance << "\n"
        << "DoF size      : " << (mpImpl->mMaybeDoFCount.has_value() ? std::to_string(mpImpl->mMaybeDoFCount.value()) : "auto") << "\n"
        << "verbosity     : " << mpImpl->mVerbosity << "\n"
        << "AMGCL settings: "
        ;
    boost::property_tree::json_parser::write_json(rStream, mpImpl->mAMGCLSettings);
}



template class KRATOS_API(UTILITY_APPLICATION)
AMGCLWrapper<
    TUblasSparseSpace<double>,
    TUblasDenseSpace<double>,
    Reorderer<
        TUblasSparseSpace<double>,
        TUblasDenseSpace<double>
    >
>;


template class KRATOS_API(UTILITY_APPLICATION)
AMGCLWrapper<
    TUblasSparseSpace<float>,
    TUblasDenseSpace<double>,
    Reorderer<
        TUblasSparseSpace<float>,
        TUblasDenseSpace<double>
    >
>;


} // namespace Kratos
