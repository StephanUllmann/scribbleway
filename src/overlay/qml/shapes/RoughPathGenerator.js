.pragma library

function createPRNG(seed) {
    let s = seed;
    return function() {
        s = (s * 9301 + 49297) % 233280;
        return s / 233280.0;
    };
}

function factor(roughness, base) {
    // base: default 2 for lines/arcs/ellipses; freehand uses 1.5
    let b = base === undefined ? 2.0 : base;
    return roughness === 1 ? b : b * 2.0;
}

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

function getSketchyArc(cx, cy, R, startAngle, endAngle, roughness, rand) {
    if (R <= 0 || roughness === 0) return [];
    let strokes = [];
    let roughnessFactor = factor(roughness);

    for (let loop = 0; loop < 2; ++loop) {
        let pts = [];
        let steps = 8;
        let phase = rand() * 2 * Math.PI;
        for (let i = 0; i <= steps; ++i) {
            let t = i / steps;
            let theta = startAngle + t * (endAngle - startAngle);
            let offset = roughnessFactor * (0.6 + rand() * 0.8);
            let rVar = R + Math.sin(theta * 2 + phase) * offset * 0.8;

            let jx = (rand() - 0.5) * offset * 0.3;
            let jy = (rand() - 0.5) * offset * 0.3;

            let px = cx + rVar * Math.cos(theta) + jx;
            let py = cy + rVar * Math.sin(theta) + jy;
            pts.push(Qt.point(px, py));
        }
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

    strokes = strokes.concat(getSketchyArc(x + w - R, y + R, R, -Math.PI / 2, 0, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + w - R, y + h - R, R, 0, Math.PI / 2, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + R, y + h - R, R, Math.PI / 2, Math.PI, roughness, rand));
    strokes = strokes.concat(getSketchyArc(x + R, y + R, R, Math.PI, 3 * Math.PI / 2, roughness, rand));

    return strokes;
}

function getSketchyEllipse(x, y, w, h, roughness, seed) {
    if (roughness === 0) return [];
    let cx = x + w / 2;
    let cy = y + h / 2;
    let rx = w / 2;
    let ry = h / 2;
    if (rx < 1 || ry < 1) return [];

    let rand = createPRNG(seed);
    let strokes = [];
    let roughnessFactor = factor(roughness);

    for (let loop = 0; loop < 2; ++loop) {
        let pts = [];
        let steps = 32;
        let phaseX = rand() * 2 * Math.PI;
        let phaseY = rand() * 2 * Math.PI;

        let extraSteps = 3;
        for (let i = 0; i <= steps + extraSteps; ++i) {
            let theta = (i / steps) * 2 * Math.PI;
            let offset = roughnessFactor * (0.6 + rand() * 0.8);
            let rxVar = rx + Math.sin(theta * 2 + phaseX) * offset * 1.5;
            let ryVar = ry + Math.cos(theta * 2 + phaseY) * offset * 1.5;

            let jx = (rand() - 0.5) * offset * 0.3;
            let jy = (rand() - 0.5) * offset * 0.3;

            let px = cx + rxVar * Math.cos(theta) + jx;
            let py = cy + ryVar * Math.sin(theta) + jy;
            pts.push(Qt.point(px, py));
        }
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

function getSketchyFreehand(points, roughness, seed) {
    if (!points || points.length < 2 || roughness === 0) return [];

    let rand = createPRNG(seed);
    let strokes = [];
    let roughnessFactor = factor(roughness, 1.5);

    for (let loop = 0; loop < 2; ++loop) {
        let pts = [];
        let phase = rand() * 2 * Math.PI;
        for (let i = 0; i < points.length; ++i) {
            let offset = roughnessFactor * (0.5 + rand() * 0.5);
            let jx = Math.sin(i * 0.5 + phase) * offset * 0.7 + (rand() - 0.5) * offset * 0.3;
            let jy = Math.cos(i * 0.5 + phase) * offset * 0.7 + (rand() - 0.5) * offset * 0.3;
            pts.push(Qt.point(points[i].x + jx, points[i].y + jy));
        }
        strokes.push(pts);
    }
    return strokes;
}
