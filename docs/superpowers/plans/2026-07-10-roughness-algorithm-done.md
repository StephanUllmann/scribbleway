# Excalidraw-like Roughness Algorithm Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement true Excalidraw-like roughness/sloppiness (bowing, cubic Bezier curves, and Catmull-Rom splines) evaluated and flattened in Javascript for all Scribbleway drawn shapes.

**Architecture:** We will replace the linear/jagged shape generation logic in `RoughPathGenerator.js` with the mathematical algorithms from RoughJS. By evaluating Bezier curves and splines into discrete point lists (`Qt.point`) directly in Javascript, we render them smoothly through QML's existing `RoughStroke` component without altering any QML layout files.

**Tech Stack:** QML / QtQuick / Javascript

## Global Constraints
- Do not modify QML structure or interface contracts in `RoughStroke.qml`.
- Retain deterministic rendering using the existing Linear Congruential Generator (LCG) PRNG.
- Keep C++ tests passing.

---

### Task 1: Mathematical Helpers

Implement the basic mathematical helpers in `RoughPathGenerator.js` for offset calculations, cubic Bezier evaluation, and Catmull-Rom spline evaluation.

**Files:**
- Modify: `src/overlay/qml/shapes/RoughPathGenerator.js`

**Interfaces:**
- Consumes: Nothing
- Produces:
  - `evaluateBezier(p0, p1, p2, p3, t)`
  - `evaluateBezierCurve(p0, p1, p2, p3, steps)`
  - `evaluateCatmullRomSplineDirect(points, steps, tightness)`
  - `evaluateCatmullRomSpline(points, steps, tightness)`
  - `_offset(min, max, roughness, rand, roughnessGain)`
  - `_offsetOpt(x, roughness, rand, roughnessGain)`

- [ ] **Step 1: Replace/extend the helper functions at the top of the file**

Add the helper functions to `src/overlay/qml/shapes/RoughPathGenerator.js`:
```javascript
function _offset(min, max, roughness, rand, roughnessGain) {
    let rGain = roughnessGain === undefined ? 1.0 : roughnessGain;
    return roughness * rGain * (rand() * (max - min) + min);
}

function _offsetOpt(x, roughness, rand, roughnessGain) {
    return _offset(-x, x, roughness, rand, roughnessGain);
}

function evaluateBezier(p0, p1, p2, p3, t) {
    let t1 = 1.0 - t;
    let x = t1 * t1 * t1 * p0.x + 3.0 * t1 * t1 * t * p1.x + 3.0 * t1 * t * t * p2.x + t * t * t * p3.x;
    let y = t1 * t1 * t1 * p0.y + 3.0 * t1 * t1 * t * p1.y + 3.0 * t1 * t * t * p2.y + t * t * t * p3.y;
    return Qt.point(x, y);
}

function evaluateBezierCurve(p0, p1, p2, p3, steps) {
    let pts = [];
    for (let i = 0; i <= steps; ++i) {
        pts.push(evaluateBezier(p0, p1, p2, p3, i / steps));
    }
    return pts;
}

function evaluateCatmullRomSplineDirect(points, steps, tightness) {
    if (points.length < 4) return [];
    let s = 1.0 - (tightness === undefined ? 0.0 : tightness);
    
    let pts = [];
    pts.push(points[1]);
    for (let i = 1; i < points.length - 2; ++i) {
        let p0 = points[i - 1];
        let p1 = points[i];
        let p2 = points[i + 1];
        let p3 = points[i + 2];
        
        let cp1 = Qt.point(
            p1.x + (s * p2.x - s * p0.x) / 6.0,
            p1.y + (s * p2.y - s * p0.y) / 6.0
        );
        let cp2 = Qt.point(
            p2.x + (s * p1.x - s * p3.x) / 6.0,
            p2.y + (s * p1.y - s * p3.y) / 6.0
        );
        
        for (let j = 1; j <= steps; ++j) {
            pts.push(evaluateBezier(p1, cp1, cp2, p2, j / steps));
        }
    }
    return pts;
}

function evaluateCatmullRomSpline(points, steps, tightness) {
    if (points.length < 2) return [];
    let ps = [];
    ps.push(points[0]);
    for (let i = 0; i < points.length; ++i) {
        ps.push(points[i]);
    }
    ps.push(points[points.length - 1]);
    return evaluateCatmullRomSplineDirect(ps, steps, tightness);
}
```

- [ ] **Step 2: Commit mathematical helpers**

```bash
git add src/overlay/qml/shapes/RoughPathGenerator.js
git commit -m "feat: implement Bezier and Catmull-Rom spline mathematical helpers"
```

---

### Task 2: Refactor Lines and Arrows

Update the line and arrow rendering logic to use cubic Bezier curves with bowing.

**Files:**
- Modify: `src/overlay/qml/shapes/RoughPathGenerator.js`

**Interfaces:**
- Consumes: Mathematical helpers from Task 1
- Produces: Updated `getSketchyLineWithPRNG`, `getSketchyLine`, and `getSketchyArrow`

- [ ] **Step 1: Replace line and arrow functions**

Rewrite the following functions in `src/overlay/qml/shapes/RoughPathGenerator.js`:
```javascript
function getSketchyLine(x1, y1, x2, y2, roughness, seed) {
    if (roughness === 0) return [];
    return getSketchyLineWithPRNG(x1, y1, x2, y2, roughness, createPRNG(seed));
}

function getSketchyLineWithPRNG(x1, y1, x2, y2, roughness, rand) {
    if (roughness === 0) return [];

    let lengthSq = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
    let length = Math.sqrt(lengthSq);
    if (length < 2) return [];

    let roughnessGain = 1.0;
    if (length < 200) {
        roughnessGain = 1.0;
    } else if (length > 500) {
        roughnessGain = 0.4;
    } else {
        roughnessGain = (-0.0016668) * length + 1.233334;
    }

    let offset = 2.0;
    if ((offset * offset * 100) > lengthSq) {
        offset = length / 10.0;
    }
    let halfOffset = offset / 2.0;

    let strokes = [];

    for (let loop = 0; loop < 2; ++loop) {
        let isOverlay = (loop === 1);
        let currentOffset = isOverlay ? halfOffset : offset;

        let bowing = 1.0;
        let midDispX = bowing * offset * (y2 - y1) / 200.0;
        let midDispY = bowing * offset * (x1 - x2) / 200.0;
        midDispX = _offsetOpt(midDispX, roughness, rand, roughnessGain);
        midDispY = _offsetOpt(midDispY, roughness, rand, roughnessGain);

        let divergePoint = 0.2 + rand() * 0.2;

        let startPoint = Qt.point(
            x1 + _offsetOpt(currentOffset, roughness, rand, roughnessGain),
            y1 + _offsetOpt(currentOffset, roughness, rand, roughnessGain)
        );

        let cp1 = Qt.point(
            midDispX + x1 + (x2 - x1) * divergePoint + _offsetOpt(currentOffset, roughness, rand, roughnessGain),
            midDispY + y1 + (y2 - y1) * divergePoint + _offsetOpt(currentOffset, roughness, rand, roughnessGain)
        );

        let cp2 = Qt.point(
            midDispX + x1 + 2.0 * (x2 - x1) * divergePoint + _offsetOpt(currentOffset, roughness, rand, roughnessGain),
            midDispY + y1 + 2.0 * (y2 - y1) * divergePoint + _offsetOpt(currentOffset, roughness, rand, roughnessGain)
        );

        let endPoint = Qt.point(
            x2 + _offsetOpt(currentOffset, roughness, rand, roughnessGain),
            y2 + _offsetOpt(currentOffset, roughness, rand, roughnessGain)
        );

        let pts = evaluateBezierCurve(startPoint, cp1, cp2, endPoint, 12);
        strokes.push(pts);
    }
    return strokes;
}

function getSketchyArrow(fromX, fromY, toX, toY, roughness, seed, arrowLength) {
    if (roughness === 0) return [];
    let rand = createPRNG(seed);
    let strokes = [];

    strokes = strokes.concat(getSketchyLineWithPRNG(fromX, fromY, toX, toY, roughness, rand));

    let lineAngle = Math.atan2(toY - fromY, toX - fromX);
    let len = arrowLength === undefined ? 12.0 : arrowLength;
    let arrowHalfAngle = Math.PI / 6.0;

    let arrowLeftX = toX - len * Math.cos(lineAngle - arrowHalfAngle);
    let arrowLeftY = toY - len * Math.sin(lineAngle - arrowHalfAngle);

    let arrowRightX = toX - len * Math.cos(lineAngle + arrowHalfAngle);
    let arrowRightY = toY - len * Math.sin(lineAngle + arrowHalfAngle);

    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowLeftX, arrowLeftY, roughness, rand));
    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowRightX, arrowRightY, roughness, rand));

    return strokes;
}
```

- [ ] **Step 2: Commit lines and arrows changes**

```bash
git add src/overlay/qml/shapes/RoughPathGenerator.js
git commit -m "refactor: update lines and arrows using cubic Beziers and bowing"
```

---

### Task 3: Refactor Curves, Ellipses, and Arcs

Update the ellipse, arc, and freehand rendering to use Catmull-Rom splines.

**Files:**
- Modify: `src/overlay/qml/shapes/RoughPathGenerator.js`

**Interfaces:**
- Consumes: Mathematical helpers from Task 1
- Produces: Updated `getSketchyEllipse`, `getSketchyArc`, `getSketchyFreehand`, and `getSketchyRectangle`

- [ ] **Step 1: Replace ellipse, arc, freehand, and rectangle functions**

Rewrite the remaining functions in `src/overlay/qml/shapes/RoughPathGenerator.js`:
```javascript
function getSketchyEllipse(x, y, w, h, roughness, seed) {
    if (roughness === 0) return [];
    let cx = x + w / 2.0;
    let cy = y + h / 2.0;
    let rx = Math.abs(w / 2.0);
    let ry = Math.abs(h / 2.0);
    if (rx < 1 || ry < 1) return [];

    let rand = createPRNG(seed);
    let strokes = [];

    let psq = Math.sqrt(Math.PI * 2.0 * Math.sqrt((rx * rx + ry * ry) / 2.0));
    let stepCount = Math.ceil(Math.max(9, (9 / Math.sqrt(200.0)) * psq));
    let increment = (Math.PI * 2.0) / stepCount;

    let overlap = increment * _offset(0.1, _offset(0.4, 1.0, roughness, rand), roughness, rand);

    for (let loop = 0; loop < 2; ++loop) {
        let offsetScale = (loop === 0) ? 1.0 : 1.5;
        let radOffset = _offsetOpt(0.5, roughness, rand) - (Math.PI / 2.0);
        
        let points = [];
        
        points.push(Qt.point(
            _offsetOpt(offsetScale, roughness, rand) + cx + 0.9 * rx * Math.cos(radOffset - increment),
            _offsetOpt(offsetScale, roughness, rand) + cy + 0.9 * ry * Math.sin(radOffset - increment)
        ));

        let endAngle = Math.PI * 2.0 + radOffset - 0.01;
        for (let angle = radOffset; angle < endAngle; angle += increment) {
            points.push(Qt.point(
                _offsetOpt(offsetScale, roughness, rand) + cx + rx * Math.cos(angle),
                _offsetOpt(offsetScale, roughness, rand) + cy + ry * Math.sin(angle)
            ));
        }

        points.push(Qt.point(
            _offsetOpt(offsetScale, roughness, rand) + cx + rx * Math.cos(radOffset + Math.PI * 2.0 + overlap * 0.5),
            _offsetOpt(offsetScale, roughness, rand) + cy + ry * Math.sin(radOffset + Math.PI * 2.0 + overlap * 0.5)
        ));
        points.push(Qt.point(
            _offsetOpt(offsetScale, roughness, rand) + cx + 0.98 * rx * Math.cos(radOffset + overlap),
            _offsetOpt(offsetScale, roughness, rand) + cy + 0.98 * ry * Math.sin(radOffset + overlap)
        ));
        points.push(Qt.point(
            _offsetOpt(offsetScale, roughness, rand) + cx + 0.9 * rx * Math.cos(radOffset + overlap * 0.5),
            _offsetOpt(offsetScale, roughness, rand) + cy + 0.9 * ry * Math.sin(radOffset + overlap * 0.5)
        ));

        let pts = evaluateCatmullRomSplineDirect(points, 8);
        strokes.push(pts);
    }
    return strokes;
}

function getSketchyArc(cx, cy, R, startAngle, endAngle, roughness, rand) {
    if (R <= 0 || roughness === 0) return [];
    let strokes = [];

    let curveStepCount = 9;
    let ellipseInc = (Math.PI * 2.0) / curveStepCount;
    let arcInc = Math.min(ellipseInc / 2.0, (endAngle - startAngle) / 2.0);

    for (let loop = 0; loop < 2; ++loop) {
        let offsetScale = (loop === 0) ? 1.0 : 1.5;
        let radOffset = startAngle + _offsetOpt(0.1, roughness, rand);
        let points = [];
        
        points.push(Qt.point(
            _offsetOpt(offsetScale, roughness, rand) + cx + 0.9 * R * Math.cos(radOffset - arcInc),
            _offsetOpt(offsetScale, roughness, rand) + cy + 0.9 * R * Math.sin(radOffset - arcInc)
        ));

        for (let angle = radOffset; angle <= endAngle; angle += arcInc) {
            points.push(Qt.point(
                _offsetOpt(offsetScale, roughness, rand) + cx + R * Math.cos(angle),
                _offsetOpt(offsetScale, roughness, rand) + cy + R * Math.sin(angle)
            ));
        }

        points.push(Qt.point(
            cx + R * Math.cos(endAngle),
            cy + R * Math.sin(endAngle)
        ));
        points.push(Qt.point(
            cx + R * Math.cos(endAngle),
            cy + R * Math.sin(endAngle)
        ));

        let pts = evaluateCatmullRomSplineDirect(points, 8);
        strokes.push(pts);
    }
    return strokes;
}

function getSketchyRectangle(x, y, w, h, roughness, seed, borderRadius) {
    if (roughness === 0) return [];
    let R = borderRadius || 0;
    let rand = createPRNG(seed);
    let strokes = [];

    if (R <= 0) {
        strokes = strokes.concat(getSketchyLineWithPRNG(x, y, x + w, y, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x + w, y, x + w, y + h, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x + w, y + h, x, y + h, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x, y + h, x, y, roughness, rand));
        return strokes;
    }

    strokes = strokes.concat(getSketchyLineWithPRNG(x + R, y, x + w - R, y, roughness, rand));
    strokes = strokes.concat(getSketchyLineWithPRNG(x + w, y + R, x + w, y + h - R, roughness, rand));
    strokes = strokes.concat(getSketchyLineWithPRNG(x + w - R, y + h, x + R, y + h, roughness, rand));
    strokes = strokes.concat(getSketchyLineWithPRNG(x, y + h - R, x, y + R, roughness, rand));

    strokes = strokes.concat(getSketchyArc(x + w - R, y + R, R, -Math.PI / 2.0, 0, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + w - R, y + h - R, R, 0, Math.PI / 2.0, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + R, y + h - R, R, Math.PI / 2.0, Math.PI, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + R, y + R, R, Math.PI, 3.0 * Math.PI / 2.0, roughness, rand));

    return strokes;
}

function getSketchyFreehand(points, roughness, seed) {
    if (!points || points.length < 2 || roughness === 0) return [];

    let rand = createPRNG(seed);
    let strokes = [];

    for (let loop = 0; loop < 2; ++loop) {
        let offsetScale = (loop === 0) ? 1.0 : 1.5;
        let pts = [];
        for (let i = 0; i < points.length; ++i) {
            let offset = _offsetOpt(offsetScale * 1.5, roughness, rand);
            pts.push(Qt.point(
                points[i].x + offset,
                points[i].y + offset
            ));
        }
        let strokePts = evaluateCatmullRomSpline(pts, 8);
        strokes.push(strokePts);
    }
    return strokes;
}
```

- [ ] **Step 2: Commit curves, ellipses, and arcs changes**

```bash
git add src/overlay/qml/shapes/RoughPathGenerator.js
git commit -m "refactor: update ellipses, arcs, and freehand drawing using Catmull-Rom splines"
```

---

### Task 4: Run Verification Tests

Run the existing C++ unit tests to ensure that no data model properties or serialization constraints were broken.

**Files:**
- Test: `tests/shapesmodeltest.cpp`

- [ ] **Step 1: Build the test suite**

Run:
```bash
cmake -B build -DBUILD_PLASMA_APPLET=OFF -DBUILD_TESTING=ON && cmake --build build
```
Expected: Compiles successfully without warnings or errors.

- [ ] **Step 2: Run tests**

Run:
```bash
./build/tests/shapesmodeltest
```
Expected: PASS all tests, including `testRoughness`.
