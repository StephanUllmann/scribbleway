# Specification: Excalidraw-like Roughness Algorithm

This specification describes the transition from Scribbleway's current linear/jagged hand-drawn shape generator to a smooth, organic, Bezier-based rendering algorithm mirroring **RoughJS** (the engine behind **Excalidraw**).

## 1. Problem Statement
The current implementation in `RoughPathGenerator.js` produces sketchy shapes by subdividing lines into straight segments and applying jagged perpendicular offsets. While this introduces randomness, it lacks the flowing, organic "bowing" and curved perimeters that make Excalidraw's shapes feel hand-drawn.

## 2. Proposed Architecture & Algorithms
We will implement the RoughJS line-drawing and curve-interpolation mathematics directly in Javascript. To keep QML's rendering layer fast and simple, all curves (Bezier curves and Catmull-Rom splines) will be evaluated/flattened into lists of coordinates (`Qt.point`) in Javascript and returned to QML as polyline paths.

### 2.1 Cubic Bezier Evaluation
For smooth stroke drawing, we will implement a cubic Bezier evaluator. Given a start point $P_0$, control points $P_1, P_2$, and end point $P_3$, a point at time $t \in [0, 1]$ is:
$$B(t) = (1-t)^3 P_0 + 3(1-t)^2 t P_1 + 3(1-t) t^2 P_2 + t^3 P_3$$

Lines are subdivided into $12$ segments (evaluating $t$ at $13$ discrete steps: $0, \frac{1}{12}, \frac{2}{12}, \dots, 1$) to produce smooth curves.

### 2.2 Line Bowing & Double Strokes
For a line from $(x_1, y_1)$ to $(x_2, y_2)$:
1. Calculate line length $L = \sqrt{(x_2-x_1)^2 + (y_2-y_1)^2}$.
2. Determine a length-dependent `roughnessGain`:
   $$\text{gain} = \begin{cases} 1 & L < 200 \\ 0.4 & L > 500 \\ -0.0016668 \times L + 1.233334 & \text{otherwise} \end{cases}$$
3. Compute perpendicular bowing displacement vectors scaled by the `bowing` factor (default `1.0`):
   $$\text{midDispX} = \text{bowing} \times \text{maxOffset} \times \frac{y_2 - y_1}{200}$$
   $$\text{midDispY} = \text{bowing} \times \text{maxOffset} \times \frac{x_1 - x_2}{200}$$
   These vectors are randomized using our LCG PRNG scaled by $\text{gain}$.
4. A diverge point $d = 0.2 + \text{rand}() \times 0.2$ is chosen. Control points $P_1$ and $P_2$ are placed at fractions $d$ and $2d$ along the line, offset by the randomized bowing vectors and random coordinate noise.
5. Two passes are generated: a main stroke and an overlay stroke with slightly altered random seeds (as in RoughJS's `_doubleLine` behavior), creating a natural overlapping effect.

### 2.3 Catmull-Rom Spline Interpolation (for Ellipses and Freehand)
For curved paths (ellipses, arcs, freehand lines), we generate a list of vertices with random offsets and interpolate between them using a Catmull-Rom spline. 
For each segment from $P_i$ to $P_{i+1}$:
- We calculate control points:
  $$CP_1 = P_i + \frac{1 - \text{tightness}}{6} (P_{i+1} - P_{i-1})$$
  $$CP_2 = P_{i+1} - \frac{1 - \text{tightness}}{6} (P_{i+2} - P_i)$$
- We evaluate this segment as a cubic Bezier curve with control points $CP_1, CP_2$ at $8$ steps, producing smooth continuous curves.

---

## 3. Detailed Changes in `RoughPathGenerator.js`
We will rewrite `RoughPathGenerator.js` to include the following helper and generator functions:

### 3.1 Mathematical Helpers
* `evaluateBezier(p0, p1, p2, p3, t)`: returns a `Qt.point` at parameter $t$.
* `evaluateBezierCurve(p0, p1, p2, p3, steps)`: returns an array of points evaluating the cubic Bezier.
* `evaluateCatmullRomSpline(points, steps, tightness)`: takes a list of points, generates Catmull-Rom cubic Bezier control points for each segment, evaluates them, and returns a unified flat array of points.

### 3.2 Main API Functions
* `getSketchyLine(x1, y1, x2, y2, roughness, seed)`:
  Generates two smooth Bezier strokes connecting $(x_1, y_1)$ and $(x_2, y_2)$ using the bowing math.
* `getSketchyRectangle(x, y, w, h, roughness, seed, borderRadius)`:
  Generates four separate overlapping Bezier lines. If `borderRadius > 0`, it generates four straight segments and four Catmull-Rom arcs.
* `getSketchyEllipse(x, y, w, h, roughness, seed)`:
  Generates perturbed ellipse coordinates using $\cos(\theta)$ and $\sin(\theta)$ with radial offsets, then runs them through `evaluateCatmullRomSpline` to get a continuous smooth loop.
* `getSketchyArc(cx, cy, R, startAngle, endAngle, roughness, rand)`:
  Generates perturbed arc coordinates and interpolates using `evaluateCatmullRomSpline`.
* `getSketchyArrow(fromX, fromY, toX, toY, roughness, seed, arrowLength)`:
  Generates a bowing line for the shaft and two bowing lines for the arrowheads.
* `getSketchyFreehand(points, roughness, seed)`:
  Runs the raw stylus/mouse points through `evaluateCatmullRomSpline` (applying random offsets to each point).

---

## 4. Verification Plan
* **Unit Tests (`tests/shapesmodeltest.cpp`):** Verify that the updated generator successfully runs and returns valid coordinate arrays.
* **Visual Verification:** Build the application and visually confirm in the overlay interface that:
  - Rectangles and lines are rendered with smooth curves/bowing rather than sharp jagged angles.
  - Ellipses and freehand strokes are smooth and organic.
  - The double-stroke overlapping effect is visible.
  - Border radius is preserved on rounded rectangles.
