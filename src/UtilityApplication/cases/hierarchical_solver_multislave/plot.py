# --- External Imports ---
import numpy
import scipy
from matplotlib import pyplot

# --- STD Imports ---
import pathlib
import re
import argparse


parser = argparse.ArgumentParser(pathlib.Path(__file__).stem)
parser.add_argument("directory",
                    type = pathlib.Path,
                    default = pathlib.Path("."))
arguments = parser.parse_args()

system_matrix = scipy.io.mmread(arguments.directory / "system_matrix.mm").todense()
eigenvalues, eigenvectors = numpy.linalg.eig(system_matrix)
indices = eigenvalues.argsort()[::-1]
eigenvalues = eigenvalues[indices]
eigenvectors = eigenvectors[indices]
eigenvectors = numpy.array(eigenvectors)
error_norm = numpy.linalg.norm(scipy.io.mmread(arguments.directory / "rhs.mm"))

pattern = R"\w+?([0-9]+)\.mm"
regex = re.compile(pattern)

presmoothed_deltas = []
prolonged_deltas = []
postsmoothed_deltas = []
solutions = []

for file_name in arguments.directory.glob("presmoothed_delta_*.mm"):
    index = int(regex.match(str(file_name.name)).group(1))
    presmoothed_deltas.append((index,
                               scipy.io.mmread(file_name)))

for file_name in arguments.directory.glob("prolonged_delta_*.mm"):
    index = int(regex.match(str(file_name.name)).group(1))
    prolonged_deltas.append((index,
                                scipy.io.mmread(file_name)))

for file_name in arguments.directory.glob("postsmoothed_delta_*.mm"):
    index = int(regex.match(str(file_name.name)).group(1))
    postsmoothed_deltas.append((index,
                           scipy.io.mmread(file_name)))

for file_name in arguments.directory.glob("solution_*.mm"):
    index = int(regex.match(str(file_name.name)).group(1))
    solutions.append((index,
                      scipy.io.mmread(file_name)))

presmoothed_deltas = [entry[1] for entry in sorted(presmoothed_deltas, key = lambda entry: entry[0])]
prolonged_deltas = [entry[1] for entry in sorted(prolonged_deltas, key = lambda entry: entry[0])]
postsmoothed_deltas = [entry[1] for entry in sorted(postsmoothed_deltas, key = lambda entry: entry[0])]
solutions = [entry[1] for entry in sorted(solutions, key = lambda entry: entry[0])]

max_size = max(len(container) for container in (presmoothed_deltas, prolonged_deltas, postsmoothed_deltas, solutions))
presmoothed_deltas += [None for _ in range(max_size - len(presmoothed_deltas))]
prolonged_deltas += [None for _ in range(max_size - len(prolonged_deltas))]
postsmoothed_deltas += [None for _ in range(max_size - len(postsmoothed_deltas))]
solutions += [None for _ in range(max_size - len(solutions))]

final_solution = solutions[-1]

figure, (axis_top, axis_bottom) = pyplot.subplots(2)
bottom_ylim = None

for i, (presmoothed_delta, prolonged_delta, postsmoothed_delta, solution) in enumerate(zip(presmoothed_deltas, prolonged_deltas, postsmoothed_deltas, solutions)):
    legend = []

    print(i)
    delta = presmoothed_delta + prolonged_delta + postsmoothed_delta

    if presmoothed_delta is not None:
        axis_top.plot(presmoothed_delta, "c-.")
        legend.append("pre-smoothed delta")
        axis_bottom.plot(
            eigenvalues,
            [(numpy.dot(numpy.ravel(presmoothed_delta), eigenvectors[:,i_eigenvector]) - numpy.dot(numpy.ravel(delta), eigenvectors[:,i_eigenvector])) for i_eigenvector in range(eigenvectors.shape[1])],
            "c-.")

    if prolonged_delta is not None:
        axis_top.plot(prolonged_delta, "b")
        legend.append("prolonged delta")
        axis_bottom.plot(
            eigenvalues,
            [(numpy.dot(numpy.ravel(prolonged_delta), eigenvectors[:,i_eigenvector]) - numpy.dot(numpy.ravel(delta), eigenvectors[:,i_eigenvector])) for i_eigenvector in range(eigenvectors.shape[1])],
            "b")

    if postsmoothed_delta is not None:
        axis_top.plot(postsmoothed_delta, "g")
        legend.append("post-smoothed delta")
        axis_bottom.plot(
            eigenvalues,
            [(numpy.dot(numpy.ravel(postsmoothed_delta), eigenvectors[:,i_eigenvector]) - numpy.dot(numpy.ravel(delta), eigenvectors[:,i_eigenvector])) for i_eigenvector in range(eigenvectors.shape[1])],
            "g")

    if solution is not None:
        axis_top.plot(solution, "r--")
        legend.append("solution")

        axis_bottom.plot(
            eigenvalues,
            [(numpy.dot(numpy.ravel(solution), eigenvectors[:,i_eigenvector]) - numpy.dot(numpy.ravel(final_solution), eigenvectors[:,i_eigenvector])) for i_eigenvector in range(eigenvectors.shape[1])],
            "r--")
        if bottom_ylim is not None:
            axis_bottom.set_ylim(bottom_ylim)
        else:
            bottom_ylim = axis_bottom.get_ylim()

    axis_top.plot(final_solution, "k-.")
    legend.append("final solution")

    #axis_top.gca().set_yscale("log")
    #axis_top.xticks([v for v in range(len(solution))])
    axis_top.grid(True, "major", "x")
    axis_top.legend(legend)
    axis_top.set_title(f"solution at iteration {i}")

    axis_bottom.set_xscale("log")
    axis_bottom.set_title(f"decomposed error at iteration {i}")

    figure.tight_layout()
    figure.savefig(arguments.directory / f"solution_{i}.png")
    axis_top.clear()
    axis_bottom.clear()
