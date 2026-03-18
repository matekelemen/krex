# AILU

Example case for a framework to experiment with training and using ML-based preconditioners in *KratosMultiphysics*.

## Description

Let's say you had the terrible idea of replacing your preconditioner (i.e. linear algebra) with a neural network (i.e. even more linear algebra). This example demonstrates how you'd go about training your network for a specific problem, and using the trained network to replace the preconditioner of an iterative linear solver in *KratosMultiphysics*.

## Dependencies

- `KratosMultiphysics` ([branch of a fork](https://github.com/matekelemen/kratos/tree/linearsolvers/generic-preconditioner))
    - `LinearSolversApplication`
    - `StructuralMechanicsApplication`

## Example Model

The example defines a static linear elasticity problem defined on a cube, discretized by 4 tetrahedra.
- `mesh.mdpa` defines the mesh in *KratosMultiphysics* format.
- `main.json` provides the configuration for *KratosMultiphysics*.
- `materials.json` defines the material model and its parameters in *KratosMultiphysics* format.
- `training.py` is an example script that yoinks the left-hand-side matrix from the analysis, and feeds it to a function that is meant to provide a network with training data.
    - This example doesn't actually train a network but does an incomplete decomposition instead. You'd want to replace this part with whatever format you want to store your network in.
- `inference.py` uses the results from `training.py` to generate a lower and upper triangular matrix meant to act as preconditioners for the same problem. These matrices are then fed into a *Kratos* preconditioner and injected into the analysis, which uses it to solve the problem.
    - This example simply loads the lower and upper triangular parts of the incomplete LU factorization computed in `training.py`. In general, you'd want to replace this with doing inference with your trained network.

## Example Workflow

- ```bash
  $ export KRATOS_QUIET=1
  ```
    - *Shuts Kratos up, preventing it from polluting `stdout`.*
- ```bash
  $ python3 training.py
  ```
    - *Replaces the analysis' linear solver with a dummy, and yoinks the system matrix from the analysis. Afterward. it does an incomplete LU decomposition and writes the resulting lower and upper triangular matrices to `lower_triangle.mm` and `upper_triangle.mm` respectively.*
- ```bash
  $ python3 inference.py --preconditioner substitution
  Conjugate gradient linear solver with Preconditioner
    Initial Residual ratio : 0.0215267
    Final Residual ratio : 1.16045e-17
    Residual ratio : 5.39075e-16
    Slope : -0.0215267
    Tolerance : 1e-12
    Number of iterations : 1
    Maximum number of iterations : 1000
  ```
    - *Loads `lower_triangle.mm` and `upper_triangle.mm`, and injects a custom preconditioner into the analysis.*


##

For comparison, you can run the analysis with other preconditioners.
- ```bash
  $ python3 inference.py
  Conjugate gradient linear solver with Preconditioner
    Initial Residual ratio : 5.27046e+08
    Final Residual ratio : 3.32368e-05
    Residual ratio : 6.30623e-14
    Slope : -4.0542e+07
    Tolerance : 1e-12
    Number of iterations : 13
    Maximum number of iterations : 1000
  ```
    - *Runs the analysis without a preconditioner.*
- ```bash
  $ python3 inference.py --preconditioner diagonal
  Conjugate gradient linear solver with Diagonal preconditioner
    Initial Residual ratio : 1851.39
    Final Residual ratio : 2.56395e-10
    Residual ratio : 1.38488e-13
    Slope : -154.283
    Tolerance : 1e-12
    Number of iterations : 12
    Maximum number of iterations : 1000
  ```
    - *Runs the analysis with a simple diangoal preconditioner.*
- ```bash
  $ python3 inference.py --preconditioner ilu
  segmentation fault (core dumped)
  ```
    - *Runs the analysis with a incomplete LU preconditioner. Oh look, a segfault. Wow. Kratos truly is the pinnacle of research.*
- ```bash
  $ python3 inference.py --preconditioner ilu0
  Conjugate gradient linear solver with ILU0Preconditioner
    Initial Residual ratio : 0.0227562
    Final Residual ratio : 4.46178e-07
    Residual ratio : 1.96069e-05
    Slope : -2.27557e-05
    Tolerance : 1e-12
    Number of iterations : 1000
    Maximum number of iterations : 1000
  !!!!!!!!!!!! ITERATIVE SOLVER NON CONVERGED !!!!!!!!!!!!1000
  ```
    - *Runs the analysis with an (supposedly) ILU0 preconditioner. But it seems like not even ILU0 is correctly implemented in Kratos.*