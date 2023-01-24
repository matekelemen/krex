__all__ = ["MPIUnion"]

# Core imports
import KratosMultiphysics
import KratosMultiphysics.HDF5Application as HDF5


def MPIUnion(container: set, data_communicator: KratosMultiphysics.DataCommunicator) -> set:
    return set(HDF5.MPIAllGatherVStrings(list(container), data_communicator))
