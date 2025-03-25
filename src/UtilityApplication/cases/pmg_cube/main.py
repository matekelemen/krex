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
import enum
import os


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


class MeshFormat(enum.Enum):
    MDPA    = 1
    MED     = 2


def ParseMeshFormat(mesh_path: pathlib.Path) -> MeshFormat:
    suffix = mesh_path.suffix
    if suffix == ".mdpa":
        return MeshFormat.MDPA
    elif suffix == "" or suffix == ".med":
        return MeshFormat.MED
    else:
        raise ValueError(f"Unsupported mesh format: \"{suffix}\"")


def GetUniqueNodeId(model_part: KratosMultiphysics.ModelPart) -> int:
    id = 1
    for node in model_part.Nodes:
        if id <= node.Id:
            id = node.Id + 1
    return id


def ConfigureProjectParameters(parameters: KratosMultiphysics.Parameters,
                               arguments: argparse.Namespace) -> None:
    # Manipulate elements and surface conditions if the mesh is linear.
    if is_linear:
        for item in project_json["modelers"][0]["parameters"]["elements_list"]:
            item["element_name"] = "SmallDisplacementElement3D4N"
    parameters["solver_settings"]["model_import_settings"]["input_filename"] = "meshes/" + arguments.mesh

    # Disable p-multigrid if requested.
    if arguments.disable_pmg:
        parameters["solver_settings"]["builder_and_solver_settings"]["@include_json"] = "defaultbs.json"
    else:
        parameters["solver_settings"]["builder_and_solver_settings"]["@include_json"] = "pmgbs.json"


if __name__ == "__main__":
    # AnalysisStage assumes the pwd is in the main JSON's directory, so cd there.
    os.chdir(pathlib.Path(__file__).absolute().parent)

    # Parse command line arguments.
    parser = argparse.ArgumentParser("kratos")
    parser.add_argument("--parameters",
                        dest = "parameters_path",
                        type = pathlib.Path,
                        default = pathlib.Path("main.json"))
    parser.add_argument("--mesh",
                        dest = "mesh",
                        type = str,
                        default = "x1")
    parser.add_argument("--disable-pmg",
                        dest = "disable_pmg",
                        action = "store_const",
                        default = False,
                        const = True)
    arguments = parser.parse_args()
    is_linear = arguments.mesh.startswith("linear_")
    mesh_format = ParseMeshFormat(pathlib.Path(arguments.mesh))

    # Load and configure settings.
    with open(arguments.parameters_path, 'r') as parameter_file:
        project_json = json.load(parameter_file)
    ConfigureProjectParameters(project_json, arguments)

    # Convert settings into a Kratos object.
    parameters = KratosMultiphysics.Parameters(json.dumps(project_json))

    # Load and construct the analysis.
    analysis_stage_module_name = parameters["analysis_stage"].GetString()
    analysis_stage_class_name = analysis_stage_module_name.split('.')[-1]
    analysis_stage_class_name = ''.join(x.title() for x in analysis_stage_class_name.split('_'))

    analysis_stage_module = importlib.import_module(analysis_stage_module_name)
    analysis_stage_class = getattr(analysis_stage_module, analysis_stage_class_name)

    model = KratosMultiphysics.Model()
    root_model_part = model.CreateModelPart("root")
    analysis = analysis_stage_class(model, parameters)

    # Load the mesh.
    mesh_path: pathlib.Path = pathlib.Path("__file__").absolute().parent.parent / "pmg_cube" / "meshes" / arguments.mesh
    mesh_io: KratosMultiphysics.ModelPartIO
    if mesh_format == MeshFormat.MDPA:
        mesh_path = mesh_path.with_suffix("")
        mesh_io = KratosMultiphysics.ModelPartIO(mesh_path, KratosMultiphysics.ModelPartIO.READ)
    elif mesh_format == MeshFormat.MED:
        if not mesh_path.suffix:
            mesh_path =  mesh_path.with_suffix(".med")
        mesh_io = KratosMultiphysics.MedApplication.MedModelPartIO(mesh_path, KratosMultiphysics.ModelPartIO.READ)
    else:
        raise RuntimeError(f"Unhandled mesh format: {mesh_format}")

    mesh_io.ReadModelPart(root_model_part)
    root_model_part.ProcessInfo[KratosMultiphysics.DOMAIN_SIZE] = 3

    # Insert an external node into the mesh that will act as master or slave
    external_sub_model_part = root_model_part.CreateSubModelPart("external_node")
    id_external_node = GetUniqueNodeId(root_model_part)
    print(f"external node id is {id_external_node}")
    external_node: KratosMultiphysics.Node = external_sub_model_part.CreateNewNode(id_external_node, 10.0, 10.0, 20.0)

    # Run!
    analysis.Run()
