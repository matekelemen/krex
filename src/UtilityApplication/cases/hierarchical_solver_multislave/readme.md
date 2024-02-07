<p align="center">
<img src=".readme/figure.png" width=300/>
</p>

This example consists of 3 disconnected triangles that are constrained by MPCs along their overlapping edges and corners. All three triangles have overlapping corners at the bottom center (origin), where constraining with an MPC becomes more interesting.

The following two constraint systems are tested:
- *connected case*: the corner ($0$) of the main triangle ($e_0$) is the master of the other two ($3$ and $6$) overlapping corners.
- *disconnected case:* an external point ($18$), that is not part of any element, acts as the master of all 3 overlapping corners ($0$, $3$, and $6$).

In every setup, all MPCs share a common coefficient, with no offsets:
```math
\mathbf{u}_S =
\begin{bmatrix}
    \ddots & & \\
    & c & \\
    & & \ddots
\end{bmatrix}
\mathbf{u}_M
```

## Connected Case

```bash
python MainKratos.py --
```

## Disconnected Case
