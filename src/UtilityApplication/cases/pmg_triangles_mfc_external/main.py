# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.LinearSolversApplication
import KratosMultiphysics.StructuralMechanicsApplication
import KratosMultiphysics.MedApplication
import KratosMultiphysics.UtilityApplication

# --- STD Imports ---
import sys
import time
import importlib
import argparse
import json
import pathlib


def CreateAnalysisStageWithFlushInstance(cls, global_model, parameters):
    class AnalysisStageWithFlush(cls):

        def __init__(self, model,project_parameters, flush_frequency=10.0):
            super().__init__(model,project_parameters)
            self.flush_frequency = flush_frequency
            self.last_flush = time.time()
            sys.stdout.flush()

        def Initialize(self):
            super().Initialize()
            sys.stdout.flush()

        def FinalizeSolutionStep(self):
            super().FinalizeSolutionStep()

            if self.parallel_type == "OpenMP":
                now = time.time()
                if now - self.last_flush > self.flush_frequency:
                    sys.stdout.flush()
                    self.last_flush = now

    return AnalysisStageWithFlush(global_model, parameters)


if __name__ == "__main__":
    parser = argparse.ArgumentParser("kratos")
    parser.add_argument("--parameters",
                        dest = "parameters_path",
                        type = pathlib.Path,
                        default = pathlib.Path("main.json"))
    parser.add_argument("--mesh",
                        dest = "mesh",
                        type = str,
                        default = "linear")
    arguments = parser.parse_args()

    # Load settings.
    with open(arguments.parameters_path, 'r') as parameter_file:
        project_json = json.load(parameter_file)

    project_json["solver_settings"]["model_import_settings"]["input_filename"] = "meshes/" + arguments.mesh
    parameters = KratosMultiphysics.Parameters(json.dumps(project_json))

    model = KratosMultiphysics.Model()
    root_model_part = model.CreateModelPart("root")

    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    simulation = CreateAnalysisStageWithFlushInstance(analysis_stage_class, model, parameters)
    simulation.Run()
