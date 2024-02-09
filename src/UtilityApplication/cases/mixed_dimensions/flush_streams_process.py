# --- Kratos Imports ---
import KratosMultiphysics

# --- STD Imports ---
import sys
import time


class FlushStreamsProcess(KratosMultiphysics.Process):

    def __init__(self,
                 model: KratosMultiphysics.Model,
                 parameters: KratosMultiphysics.Parameters):
        super().__init__()
        parameters.ValidateAndAssignDefaults(self.GetDefaultParameters())
        self.__buffer_interval: float = parameters["interval"].GetDouble()
        self.__last_flush: float = time.time()

    def FinalizeSolutionStep(self) -> None:
        current_time = time.time()
        if self.__buffer_interval <= current_time - self.__last_flush:
            self.__Flush()
            self.__last_flush = current_time

    @classmethod
    def GetDefaultParameters(cls) -> KratosMultiphysics.Parameters:
        return KratosMultiphysics.Parameters("""{
            "interval" : 0.0
        }""")

    def __Flush(self) -> None:
        sys.stdout.flush()
        sys.stderr.flush()


def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> FlushStreamsProcess:
    return FlushStreamsProcess(model, parameters["Parameters"])
