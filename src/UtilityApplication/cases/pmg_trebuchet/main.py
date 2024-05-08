# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.LinearSolversApplication
import KratosMultiphysics.StructuralMechanicsApplication

# --- STD Imports ---
import importlib
import argparse
import json
import pathlib


if __name__ == "__main__":
    parser = argparse.ArgumentParser("kratos")
    parser.add_argument("-m",
                        "--mesh",
                        dest = "mesh",
                        type = str,
                        default = "quadratic")
    parser.add_argument("-s",
                        "--solver",
                        dest = "solver",
                        type = str,
                        default = "hierarchical_solver")
    parser.add_argument("--settings",
                        dest = "settings",
                        type = pathlib.Path,
                        default = "main.json")
    arguments = parser.parse_args()

    with open(arguments.settings, 'r') as parameter_file:
        project_json = json.load(parameter_file)
    project_json["solver_settings"]["model_import_settings"]["input_filename"] = "meshes/" + arguments.mesh
    project_json["solver_settings"]["linear_solver_settings"]["@include_json"] = "solvers/" + arguments.solver + ".json"

    parameters = KratosMultiphysics.Parameters(json.dumps(project_json))

    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    global_model = KratosMultiphysics.Model()
    simulation = analysis_stage_class(global_model, parameters)
    simulation.Run()
