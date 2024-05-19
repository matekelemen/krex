# --- External Imports ---
from matplotlib import pyplot

# --- STD Imports ---
import pathlib
import argparse
import re
import typing


script_dir = pathlib.Path(__file__).absolute().parent


class Dataset:

    def __init__(self, pattern: str, reduction: typing.Callable[[list[float]],typing.Optional[float]]) -> None:
        self.__pattern_string: str = pattern
        self.__pattern: re.Pattern = re.compile(pattern)
        self.__values: "list[float]" = []
        self.__reduction: typing.Callable[[list[float]],typing.Optional[float]] = reduction


    def Parse(self, line: str) -> bool:
        m = self.__pattern.match(line)
        if m:
            value = self.__reduction([float(v) for v in m.groups() if v])
            if value is not None:
                self.__values.append(value)
            return True
        return False


    def Clone(self) -> "Dataset":
        return Dataset(self.__pattern_string, self.__reduction)


    @property
    def value(self) -> typing.Optional[float]:
        return self.__reduction(self.__values)



def Max(values: "list[float]") -> typing.Optional[float]:
    return max(values) if values else None


def Sum(values: "list[float]") -> typing.Optional[float]:
    return sum(values, 0.0) if values else None


def Avg(values: "list[float]") -> typing.Optional[float]:
    return (sum(values, 0.0) / len(values)) if values else None



float_pattern = R"(-?(?:(?:(?:[1-9][0-9]*)(?:\.[0-9]*)?)|(?:0(?:\.[0-9]*)?))(?:[eE][\+-]?[0-9]+)?)"

datasets: "dict[str,Dataset]" = {
    "node_count" : Dataset(R".*Number of Nodes +: ([0-9]+)", Max),
    "element_count" : Dataset(R".*Number of Elements +: ([0-9]+)", Max),
    "condition_count" : Dataset(R".*Number of Conditions +: ([0-9]+)", Max),
    "read_time" : Dataset(R""".*Reading file ".*" took """ + float_pattern + R""" \[s\]""", Max),
    "allocation_time" : Dataset(f".*System Construction Time: {float_pattern}", Max),
    "assembly_time" : Dataset(f"ResidualBasedBlockBuilderAndSolver: (?:Build time: {float_pattern}|Constraints build time: {float_pattern})", Sum),
    "solution_time" : Dataset(f".*System solve time: {float_pattern}", Sum)
}
dataset_names: "list[str]" = list(datasets.keys())


class Case:

    __name_pattern = re.compile(R"(linear_)?x([0-9]+)_mesh_(hierarchical_solver|standalone_amgcl_raw_solver)_((?:top|volume)_[0-9]+)")

    def __init__(self, name: str) -> None:
        m = self.__name_pattern.match(name)
        if not m:
            raise ValueError(f"invalid case name: {name}")

        self.__order: int = 1 if m.group(1) == "linear_" else 2
        self.__level: int = int(m.group(2))
        self.__solver: str = "amg" if m.group(3) == "standalone_amgcl_raw_solver" else "pmg"
        self.__constrained_group: str = m.group(4)
        self.__datasets: "dict[str,Dataset]" = {name : dataset.Clone() for name, dataset in datasets.items()}


    def Parse(self, file: typing.TextIO) -> None:
        for line in file:
            for dataset in self.__datasets.values():
                dataset.Parse(line)


    @property
    def order(self) -> int:
        return self.__order


    @property
    def level(self) -> int:
        return self.__level


    @property
    def solver(self) -> str:
        return self.__solver


    @property
    def constrained_group(self) -> str:
        return self.__constrained_group


    @property
    def datasets(self) -> "dict[str,Dataset]":
        return self.__datasets



class Filter:

    __keys = ["order", "level", "solver", "constrained_group"]

    def __init__(self, definition: str) -> None:
        arguments = definition.split("=")
        if len(arguments) != 2:
            raise ValueError(f"Invalid filter definition: {definition}")

        self.__key = arguments[0]
        if self.__key not in self.__keys:
            raise ValueError(f"Invalid key {self.__key} in filter {definition}")

        self.__value = arguments[1]


    def __call__(self, case: Case) -> bool:
        return str(getattr(case, self.__key)) == self.__value



parser = argparse.ArgumentParser(pathlib.Path(__file__).stem)
parser.add_argument("-x",
                    dest = "x",
                    type = str,
                    choices = dataset_names,
                    default = "node_count")
parser.add_argument("-y",
                    dest = "y",
                    type = str,
                    choices = dataset_names,
                    default = "solution_time")
parser.add_argument("--filter",
                    dest = "filters",
                    nargs = "*",
                    type=Filter,
                    default = [])
arguments = parser.parse_args()


cases: "dict[str,Case]" = {}
for path in script_dir.glob("*/log"):
    case_name = path.parent.name
    case = Case(case_name)

    include = True
    for filter in arguments.filters:
        if filter(case):
            include = False
            break

    if include:
        cases[case_name] = case
        with open(path, "r") as file:
            case.Parse(file)

x: "list[float]" = [case.datasets[arguments.x].value for case in cases.values()]
y: "list[float]" = [case.datasets[arguments.y].value for case in cases.values()]

figure, axes = pyplot.subplots()
axes.plot(x,
          y,
          "+")
axes.set_xlabel(arguments.x)
axes.set_ylabel(arguments.y)
axes.set_xscale("log")
axes.set_yscale("log")
axes.grid()

figure.savefig("plot_performance.png")
