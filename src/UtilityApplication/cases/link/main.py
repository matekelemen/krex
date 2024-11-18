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

# Fetch nodes' displacements and initial positions.
initial_configuration: "tuple[list[float],list[float],list[float]]" = ([], [], [])
deformed_configuration: "tuple[list[float],list[float],list[float]]" = ([], [], [])

node: KratosMultiphysics.Node
for node in root_model_part.Nodes:
    initial_configuration[0].append(node.X0)
    initial_configuration[1].append(node.Y0)
    initial_configuration[2].append(node.Z0)

    deformed_configuration[0].append(node.X)
    deformed_configuration[1].append(node.Y)
    deformed_configuration[2].append(node.Z)

# Plot the results if numpy and matplotlib are available.
try:
    from matplotlib import pyplot
    import numpy

    figure = pyplot.figure()
    axes = figure.add_subplot(projection = "3d")
    axes.set_aspect("equal")

    # Draw sphere
    u, v = numpy.mgrid[0:2*numpy.pi:20j, 0:numpy.pi:10j]
    x = numpy.cos(u)*numpy.sin(v)
    y = numpy.sin(u)*numpy.sin(v)
    z = numpy.cos(v)
    axes.plot_wireframe(x, y, z, color="r")

    # Define the index buffer that turns the tetrahedron
    # into a triangulated surface matplotlib expects.
    index_buffer: list[list[int]] = [[0, 1, 2],
                                     [0, 1, 3],
                                     [0, 2, 3],
                                     [1, 2, 3]]

    # Plot initial configuration.
    axes.plot_trisurf(*initial_configuration,
                      triangles = index_buffer,
                      antialiased = True,
                      color = "#0065BD55",
                      edgecolor = "#0065BDAA",
                      linewidth = 5)

    # Plot deformed configuration.
    axes.plot_trisurf(*deformed_configuration,
                      triangles = index_buffer,
                      antialiased = True,
                      color = "#E3722255",
                      edgecolor = "#E37222AA",
                      linewidth = 5)

    axes.legend(["initial configuration",
                 "deformed configuration",
                 "constraint surface"])

    pyplot.show()

except ImportError as _:
    import sys
    print("Seems like you don't have numpy or matplotlib available. Consider installing them for a quick visualization.", file = sys.stderr)
