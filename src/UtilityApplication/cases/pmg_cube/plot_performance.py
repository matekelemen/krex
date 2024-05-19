# --- External Imports ---
from matplotlib import pyplot, font_manager
import numpy

# --- STD Imports ---
import pathlib
import argparse
import re
import typing
from collections import OrderedDict
import itertools


script_dir = pathlib.Path(__file__).absolute().parent


float_pattern_string = R"-?(?:(?:(?:[1-9][0-9]*)(?:\.[0-9]*)?)|(?:0(?:\.[0-9]*)?))(?:[eE][\+-]?[0-9]+)?"
float_pattern = re.compile(f"({float_pattern_string})")
hour_pattern = f"{float_pattern_string} " + R"\[h\] "
minute_pattern = f"{float_pattern_string} " + R"\[m\] "
second_pattern = f"{float_pattern_string} " + R"\[s\]"
time_pattern = f"(?:{hour_pattern})?(?:{minute_pattern})?{second_pattern}"
partitioned_time_pattern = f"({hour_pattern})?({minute_pattern})?({second_pattern})"
float_or_time_pattern = re.compile(f"(^{float_pattern_string}$)|(^{partitioned_time_pattern}$)")


class Dataset:

    def __init__(self, pattern: str, reduction: typing.Callable[[list[float]],typing.Optional[float]]) -> None:
        self.__pattern_string: str = pattern
        self.__pattern: re.Pattern = re.compile(pattern)
        self.__values: "list[float]" = []
        self.__reduction: typing.Callable[[list[float]],typing.Optional[float]] = reduction


    def Parse(self, line: str) -> bool:
        m = self.__pattern.match(line)
        if m:
            value = self.__reduction([self.__ToFloat(v) for v in m.groups() if v])
            if value is not None:
                self.__values.append(value)
            return True
        return False


    def Clone(self) -> "Dataset":
        return Dataset(self.__pattern_string, self.__reduction)


    @property
    def value(self) -> typing.Optional[float]:
        return self.__reduction(self.__values)


    @staticmethod
    def __ToFloat(value: str) -> float:
        m = float_or_time_pattern.match(value)
        if m.group(1):
            return float(m.group(1))
        else:
            time = 0.0
            if m.group(3):
                time += 3600.0 * float(float_pattern.match(m.group(3)).group(1))
            if m.group(4):
                time += 60.0 * float(float_pattern.match(m.group(4)).group(1))
            if m.group(5):
                time += float(float_pattern.match(m.group(5)).group(1))
            return time



def Max(values: "list[float]") -> typing.Optional[float]:
    return max(values) if values else None


def Sum(values: "list[float]") -> typing.Optional[float]:
    return sum(values, 0.0) if values else None


def Avg(values: "list[float]") -> typing.Optional[float]:
    return (sum(values, 0.0) / len(values)) if values else None


datasets: "dict[str,Dataset]" = {
    "node_count" : Dataset(R".*Number of Nodes +: ([0-9]+)", Max),
    "element_count" : Dataset(R".*Number of Elements +: ([0-9]+)", Max),
    "condition_count" : Dataset(R".*Number of Conditions +: ([0-9]+)", Max),
    "read_time" : Dataset(R""".*Reading file ".*" took (""" + time_pattern + R""") \[s\]""", Max),
    "allocation_time" : Dataset(f".*System Construction Time: ({time_pattern})", Max),
    "assembly_time" : Dataset(f"ResidualBasedBlockBuilderAndSolver: (?:Build time: ({time_pattern})|Constraints build time: ({time_pattern}))", Sum),
    "solution_time" : Dataset(f".*System solve time: ({time_pattern})", Avg)
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
        self.__solver: str = "pmg" if m.group(3) == "hierarchical_solver" else "amg"
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
parser.add_argument("--group",
                    dest = "group",
                    choices = ["all", "order", "level", "solver", "constrained_group"],
                    default = "all")
arguments = parser.parse_args()


groups: "dict[typing.Union[str,float],list[Case]]" = {}
for path in pathlib.Path(".").glob("*/log"):
    case_name = path.parent.name
    case = Case(case_name)

    include = True if not arguments.filters else all(filter(case) for filter in arguments.filters)

    if include:
        with open(path, "r") as file:
            case.Parse(file)

        if arguments.group == "all":
            groups.setdefault("all", []).append(case)
        else:
            key = getattr(case, arguments.group)
            groups.setdefault(key, []).append(case)



COLORS = OrderedDict([
    ("TUMBlue",         (  0,  82, 147, 255)),
    ("TUMOrange",       (227, 114,  34, 255)),
    ("TUMGreen",        (162, 173,   0, 255)),
    ("TUMLightestBlue", (152, 198, 234, 255)),
    ("TUMGray",         (128, 128, 128, 255)),
    ("TUMWebBlue",      ( 48, 112, 179, 255)),
    ("TUMLightGray",    (218, 215, 203, 255)),
    ("TUMLightBlue",    (  0, 101, 189, 255)),
    ("TUMLighterBlue",  (100, 160, 200, 255)),
    ("TumLightestGray", (204, 204, 204, 255)),
    ("GridLineMajor",   (218, 215, 203, 128)),
    ("GridLineMinor",   (204, 204, 204, 128))
])


LINE_STYLES = OrderedDict([
    ("solid",           "solid"),
    ("dashed",          "dashed"),
    ("dashdot",         "dashdot"),
    ("dashdotdot",      (0, (3, 2, 1, 2, 1, 2))),
    ("dotted",          "dotted"),
    ("GridLineMajor",   "solid"),
    ("GridLineMinor",   "dotted")
])


LINE_WIDTHS = OrderedDict([
    ("Icon",            4),
    ("GridLineMajor",   1.5),
    ("GridLineMinor",   1.0)
])


MARKERS = [
    "+",
    "x",
    "d",
    "2",
    "4",
    "h",
    R"$\alpha$"
]


FONTS = {
    "Title"             : font_manager.FontProperties(family = "sans-serif",
                                                      size = 18),
    "AxesLabelMajor"    : font_manager.FontProperties(family = "sans-serif",
                                                      size = 14),
    "Legend"            : font_manager.FontProperties(family = "sans-serif",
                                                      size = 14),
    "AxesLabelMinor"    : font_manager.FontProperties(family = "sans-serif",
                                                      size = 12)
}


figure, axes = pyplot.subplots()

for i_group, ((group_key, cases), color) in enumerate(zip(sorted(groups.items(), key = lambda pair: pair[0]), COLORS.values())):
    x = numpy.array([case.datasets[arguments.x].value for case in cases])
    y = numpy.array([case.datasets[arguments.y].value for case in cases])
    axes.plot(x,
              y,
              color = [c / 255.0 for c in color],
              marker = MARKERS[i_group],
              markersize=12,
              linestyle="",
              label = f"{arguments.group}: {group_key}")

    slope, intercept = numpy.polyfit(x, y, 1, w = numpy.sqrt(numpy.log(x)))

    if 0 < intercept:
        fit = lambda v: intercept + slope * v
        samples = numpy.linspace(min(x), max(x), num = 2)

        axes.plot(samples,
                fit(samples),
                color = [c / 255.0 for c in color][:3] + [0.375],
                linestyle = LINE_STYLES["dashdot"])

label_font_dict = {"fontsize" : FONTS["AxesLabelMajor"].get_size(),
                   "family" : FONTS["AxesLabelMajor"].get_family(),
                   "fontstyle" : FONTS["AxesLabelMajor"].get_style()}
axes.set_xlabel(arguments.x.replace("_", " "), fontdict = label_font_dict)
axes.set_ylabel(arguments.y.replace("_", " "), fontdict = label_font_dict)
axes.set_xscale("log")
axes.set_yscale("log")

axes.grid(which = "major",
          color = [v / 255.0 for v in COLORS["GridLineMajor"]],
          linestyle = LINE_STYLES["GridLineMajor"],
          linewidth = LINE_WIDTHS["GridLineMajor"])

axes.grid(which = "minor",
          color = [v / 255.0 for v in COLORS["GridLineMinor"]],
          linestyle = LINE_STYLES["GridLineMinor"],
          linewidth = LINE_WIDTHS["GridLineMinor"])

for label in itertools.chain(axes.get_xticklabels(), axes.get_yticklabels()):
    label.set_fontproperties(FONTS["AxesLabelMinor"])

axes.legend()

figure.savefig("plot_performance.png")
