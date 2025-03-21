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


def GetUniqueNodeId(model_part: KratosMultiphysics.ModelPart) -> int:
    id = 1
    for node in model_part.Nodes:
        if id <= node.Id:
            id = node.Id + 1
    return id


if __name__ == "__main__":
    parser = argparse.ArgumentParser("kratos")
    parser.add_argument("--parameters",
                        dest = "parameters_path",
                        type = pathlib.Path,
                        default = pathlib.Path("main.json"))
    parser.add_argument("--mesh",
                        dest = "mesh",
                        type = str,
                        default = "x1")
    parser.add_argument("--mfc-coefficient",
                        dest = "mfc_coefficient",
                        type = float,
                        default = 1.0)
    parser.add_argument("--mfc-constant",
                        dest = "mfc_constant",
                        type = float,
                        default = 0.0)
    parser.add_argument("--constrained-group",
                        dest = "constrained_group",
                        type = str,
                        default = "")
    parser.add_argument("--no-mfc",
                        dest = "no_mfc",
                        action = "store_const",
                        default = False,
                        const = True)
    arguments = parser.parse_args()
    is_linear = arguments.mesh.startswith("linear_")

    # Load settings.
    with open(arguments.parameters_path, 'r') as parameter_file:
        project_json = json.load(parameter_file)

    # Manipulate elements and surface conditions if the mesh is linear.
    if is_linear:
        for item in project_json["modelers"][0]["parameters"]["elements_list"]:
            item["element_name"] = "SmallDisplacementElement3D4N"
    project_json["solver_settings"]["model_import_settings"]["input_filename"] = "meshes/" + arguments.mesh

    # Configure MFCs.
    if arguments.no_mfc:
        print(project_json["processes"]["constraints_process_list"])
        project_json["processes"]["constraints_process_list"] = [item for item in project_json["processes"]["constraints_process_list"] if item["process_name"] != "MakeMultifreedomConstraintsProcess"]
        for item in project_json["processes"]["constraints_process_list"]:
            print(item["process_name"])
    else:
        for process_settings in project_json["processes"]["constraints_process_list"]:
            if process_settings["process_name"] == "MakeMultifreedomConstraintsProcess":
                process_settings["Parameters"]["coefficient"] = arguments.mfc_coefficient
                process_settings["Parameters"]["constant"] = arguments.mfc_constant

                if arguments.constrained_group:
                    process_settings["Parameters"]["dependent_model_part_name"] = arguments.constrained_group

    parameters = KratosMultiphysics.Parameters(json.dumps(project_json))

    model = KratosMultiphysics.Model()
    root_model_part = model.CreateModelPart("root")

    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    simulation = CreateAnalysisStageWithFlushInstance(analysis_stage_class, model, parameters)
    mesh_path = pathlib.Path("meshes") / (arguments.mesh + ".med")
    KratosMultiphysics.MedApplication.MedModelPartIO(mesh_path, KratosMultiphysics.ModelPartIO.READ).ReadModelPart(root_model_part)
    root_model_part.ProcessInfo[KratosMultiphysics.DOMAIN_SIZE] = 3

    # Insert an external node into the mesh that will act as master or slave
    external_sub_model_part = root_model_part.CreateSubModelPart("external_node")
    id_external_node = GetUniqueNodeId(root_model_part)
    print(f"external node id is {id_external_node}")
    external_node: KratosMultiphysics.Node = external_sub_model_part.CreateNewNode(id_external_node, 10.0, 10.0, 20.0)

    simulation.Run()
