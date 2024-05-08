# --- External Imports ---
import numpy

# --- Core Imports ---
import KratosMultiphysics


origin = KratosMultiphysics.Node(1, 0.0, 0.0, 0.0)
geometries: "dict[int,KratosMultiphysics.Geometry]" = {
    1 : KratosMultiphysics.Triangle2D3(origin, origin, origin),
    2 : KratosMultiphysics.Triangle2D6(origin, origin, origin, origin, origin, origin),
    3 : KratosMultiphysics.Triangle2D10(origin, origin, origin, origin, origin, origin, origin, origin, origin, origin)
}

def MakeRestrictionOperator(fine_order: int, coarse_order: int, verbose: bool = False) -> numpy.ndarray:
    fine_geometry = geometries[fine_order]
    coarse_geometry = geometries[coarse_order]

    fine_nodes = fine_geometry.PointsLocalCoordinates()

    restriction_operator = numpy.zeros((coarse_geometry.PointsNumber(), fine_geometry.PointsNumber()))
    for i_point in range(fine_geometry.PointsNumber()):
        point = [fine_nodes[i_point, 0], fine_nodes[i_point, 1], fine_nodes[i_point, 2]]
        restriction_operator[:, i_point] = coarse_geometry.ShapeFunctionsValues(point)

    if verbose:
        for i_row in range(restriction_operator.shape[0]):
            print(f"{i_row}:")
            for i_column in range(restriction_operator.shape[1]):
                value = restriction_operator[i_row, i_column]
                if 1e-14 < abs(value):
                    print(f"\t{i_column}: {value}")

    return restriction_operator


restriction_3_2 = MakeRestrictionOperator(3, 2)
restriction_2_1 = MakeRestrictionOperator(2, 1)
restriction_3_1 = MakeRestrictionOperator(3, 1)

chained_restriction_3_1 = restriction_2_1.dot(restriction_3_2)
print(numpy.linalg.norm(restriction_3_1 - chained_restriction_3_1))
