# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.LinearSolversApplication
import KratosMultiphysics.StructuralMechanicsApplication

# --- STD Imports ---
import importlib
import argparse
import json


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
    parser.add_argument("--mpc-coefficient",
                        dest = "mpc_coefficient",
                        type = float,
                        default = 1.0)
    parser.add_argument("--mpc-constant",
                        dest = "mpc_constant",
                        type = float,
                        default = 0.0)
    arguments = parser.parse_args()

    with open("main.json", 'r') as parameter_file:
        project_json = json.load(parameter_file)
    project_json["solver_settings"]["model_import_settings"]["input_filename"] = "meshes/" + arguments.mesh
    project_json["solver_settings"]["linear_solver_settings"]["@include_json"] = "solvers/" + arguments.solver + ".json"
    for process_settings in project_json["processes"]["constraints_process_list"]:
        if process_settings["process_name"] == "MakeMultifreedomConstraintsProcess":
            process_settings["Parameters"]["coefficient"] = arguments.mpc_coefficient
            process_settings["Parameters"]["constant"] = arguments.mpc_constant

    parameters = KratosMultiphysics.Parameters(json.dumps(project_json))

    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    global_model = KratosMultiphysics.Model()
    simulation = analysis_stage_class(global_model, parameters)
    simulation.Run()
