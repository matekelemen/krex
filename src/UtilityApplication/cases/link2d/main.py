# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.StructuralMechanicsApplication

# --- STD Imports ---
import importlib
import pathlib
import os


# cd to the script's directory.
os.chdir(pathlib.Path(__file__).absolute().parent)

# Read settings.
parameters: KratosMultiphysics.Parameters
with open("main.json", 'r') as parameter_file:
    parameters = KratosMultiphysics.Parameters(parameter_file.read())

# Construct root containers.
model = KratosMultiphysics.Model()
root_model_part = model.CreateModelPart("root")

# Construct an AnalysisStage of the type specified in the input settings.
analysis_module_name = parameters["analysis_stage"].GetString()
analysis_class_name = analysis_module_name.split('.')[-1]
analysis_class_name = ''.join(c.title() for c in analysis_class_name.split('_'))
analysis_module = importlib.import_module(analysis_module_name)
analysis_class = getattr(analysis_module, analysis_class_name)
analysis = analysis_class(model, parameters)
analysis.Run()

# Plot the results if numpy and matplotlib are available.
try:
    from matplotlib import pyplot
    import numpy


    # Fetch nodes' displacements and initial positions.
    initial_configuration = []
    deformed_configuration = []
    node: KratosMultiphysics.Node
    for node in root_model_part.Nodes:
        initial_configuration.append([node.X0, node.Y0])
        deformed_configuration.append([node.X0 + node.GetSolutionStepValue(KratosMultiphysics.DISPLACEMENT_X),
                                       node.Y0 + node.GetSolutionStepValue(KratosMultiphysics.DISPLACEMENT_Y)])

    initial_configuration = numpy.array(initial_configuration)
    deformed_configuration = numpy.array(deformed_configuration)

    figure = pyplot.figure()
    axes = figure.add_subplot()

    # Draw circle
    constraint_surface = pyplot.Circle(deformed_configuration[2],
                                       numpy.linalg.norm(initial_configuration[1] - initial_configuration[2]),
                                       edgecolor = "#aaaaaaff",
                                       facecolor = "#ffffffff",
                                       label = "constraint surface")
    axes.add_patch(constraint_surface)
    index_buffer: list[list[int]] = [[0, 1, 3, 2]]

    # Plot initial configuration.
    initial_configuration_patches = []
    for polygon in index_buffer:
        initial_configuration_patches.append(pyplot.Polygon(initial_configuration[polygon, :],
                                                            edgecolor = "#0065BDff",
                                                            facecolor = "#0065BD22",
                                                            linewidth = 2,
                                                            label = "initial configuration"))
        axes.add_patch(initial_configuration_patches[-1])

    # Plot deformed configuration.
    deformed_configuration_patches = []
    for polygon in index_buffer:
        deformed_configuration_patches.append(pyplot.Polygon(deformed_configuration[polygon, :],
                                                             edgecolor = "#E37222ff",
                                                             facecolor = "#E3722222",
                                                             linewidth = 2,
                                                             label = "deformed configuration"))
        axes.add_patch(deformed_configuration_patches[-1])

    # Plot dirichlet nodes.
    dirichlet_patches = []
    node: KratosMultiphysics.Node
    for node in root_model_part.GetSubModelPart("dirichlet").Nodes:
        coordinates = [node.X, node.Y, node.Z]
        dirichlet_patches.append(pyplot.Rectangle([component - 5e-2 for component in coordinates],
                                              1e-1,
                                              1e-1,
                                              edgecolor = "#000000",
                                              facecolor = "#ffffff",
                                              linewidth = 2,
                                              label = "fixed nodes"))
        axes.add_patch(dirichlet_patches[-1])

    # Plot neumann nodes.
    condition: KratosMultiphysics.Condition
    neumann_patches = []
    for condition in root_model_part.GetSubModelPart("neumann").Conditions:
        neumann_value = condition[KratosMultiphysics.StructuralMechanicsApplication.POINT_LOAD]
        node = condition.GetGeometry()[0]
        displacement = node.GetSolutionStepValue(KratosMultiphysics.DISPLACEMENT)
        neumann_patches.append(pyplot.Arrow(node.X + displacement[0], node.Y + displacement[1],
                                            neumann_value[0], neumann_value[1],
                                            width = 2e-1,
                                            edgecolor = "#be1e3cff",
                                            facecolor = "#be1e3cff",
                                            linewidth = 2,
                                            label = "load"))
        axes.add_patch(neumann_patches[-1])

    axes.legend(handles = [constraint_surface]
                        + [initial_configuration_patches[0]]
                        + [deformed_configuration_patches[0]]
                        + [dirichlet_patches[0]]
                        + [neumann_patches[0]],
                ncol = 1)
    axes.autoscale()
    axes.set_aspect("equal")
    figure.tight_layout()
    figure.savefig("link2D.png")
    #pyplot.show()

except ImportError as _:
    import sys
    print("Seems like you don't have numpy or matplotlib available. Consider installing them for a quick visualization.", file = sys.stderr)
