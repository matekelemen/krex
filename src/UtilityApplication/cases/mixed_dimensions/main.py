# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.StructuralMechanicsApplication

# --- STD Imports ---
import importlib
import argparse
import pathlib
import os


if __name__ == "__main__":
    script_directory = pathlib.Path(__file__).absolute().parent
    os.chdir(str(script_directory))

    parser = argparse.ArgumentParser("KratosMultiphysics")
    parser.add_argument("project_parameters_path",
                        type = pathlib.Path,
                        help = "path to the project parameters JSON")
    arguments = parser.parse_args()

    with open(arguments.project_parameters_path, 'r') as file:
        parameters = KratosMultiphysics.Parameters(file.read())

    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    model = KratosMultiphysics.Model()
    simulation = analysis_stage_class(model, parameters)
    simulation.Run()
