.pragma library

// rough.js draws every edge twice; with one pass the result reads as a confident pen line, not a scribble.
var STROKE_PASSES = 1;

function createPRNG(seed) {
    let s = seed;
    return function() {
        s = (s * 9301 + 49297) % 233280;
        return s / 233280.0;
    };
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

// Moves a point by up to 2px * roughness, the same amount a free line end moves.
function _jitterPoint(x, y, roughness, rand) {
    return Qt.point(x + _offsetOpt(2.0, roughness, rand), y + _offsetOpt(2.0, roughness, rand));
}

// pinEnds: the caller already jittered the ends, so neighbouring edges meet exactly.
function getSketchyLineWithPRNG(x1, y1, x2, y2, roughness, rand, forceReturnStrokes, pinEnds) {
    if (roughness === 0) return [];

    let lengthSq = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
    let length = Math.sqrt(lengthSq);
    if (length < 2 && !forceReturnStrokes) return [];

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

    for (let loop = 0; loop < STROKE_PASSES; ++loop) {
        let isOverlay = (loop === 1);
        let currentOffset = isOverlay ? halfOffset : offset;

        let bowing = 1.0;
        let midDispX = bowing * offset * (y2 - y1) / 200.0;
        let midDispY = bowing * offset * (x1 - x2) / 200.0;
        midDispX = _offsetOpt(midDispX, roughness, rand, roughnessGain);
        midDispY = _offsetOpt(midDispY, roughness, rand, roughnessGain);

        let divergePoint = 0.2 + rand() * 0.2;

        let startPoint = pinEnds ? Qt.point(x1, y1) : Qt.point(
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

        let endPoint = pinEnds ? Qt.point(x2, y2) : Qt.point(
            x2 + _offsetOpt(currentOffset, roughness, rand, roughnessGain),
            y2 + _offsetOpt(currentOffset, roughness, rand, roughnessGain)
        );

        let pts = evaluateBezierCurve(startPoint, cp1, cp2, endPoint, 12);
        strokes.push(pts);
    }
    return strokes;
}

function getSketchyRectangle(x, y, w, h, roughness, seed, borderRadius) {
    if (roughness === 0) return [];
    let R = borderRadius || 0;
    if (R > 0) {
        R = Math.min(R, Math.min(w, h) / 2.0);
    }
    let rand = createPRNG(seed);
    let strokes = [];
    // Jitter each corner once and pin every edge and rounded corner to those points, so nothing gaps.
    let c = [[x, y], [x + w, y], [x + w, y + h], [x, y + h]].map(p => _jitterPoint(p[0], p[1], roughness, rand));
    let toward = (from, to) => {
        let dx = to.x - from.x, dy = to.y - from.y, len = Math.sqrt(dx * dx + dy * dy) || 1;
        return Qt.point(from.x + R * dx / len, from.y + R * dy / len);
    };
    // Each corner is cut back by R along both edges (R = 0 leaves sharp corners).
    let ins = [], outs = [];
    for (let i = 0; i < 4; ++i) {
        ins.push(toward(c[i], c[(i + 3) % 4]));
        outs.push(toward(c[i], c[(i + 1) % 4]));
    }
    const k = 0.5523; // cubic control distance that approximates a quarter circle
    for (let i = 0; i < 4; ++i) {
        let a = outs[i], b = ins[(i + 1) % 4];
        strokes = strokes.concat(getSketchyLineWithPRNG(a.x, a.y, b.x, b.y, roughness, rand, false, true));
        if (R > 0) {
            let p = ins[i], q = outs[i], cur = c[i];
            strokes.push(evaluateBezierCurve(p,
                Qt.point(p.x + (cur.x - p.x) * k, p.y + (cur.y - p.y) * k),
                Qt.point(q.x + (cur.x - q.x) * k, q.y + (cur.y - q.y) * k), q, 8));
        }
    }
    return strokes;
}

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

    let tailRoughness = Math.min(roughness, 1);
    let overlap = increment * _offset(0.1, _offset(0.4, 1.0, tailRoughness, rand), tailRoughness, rand);

    for (let loop = 0; loop < STROKE_PASSES; ++loop) {
        let offsetScale = (loop === 0) ? 0.6 : 1.5;  // 1.0 made a single stroke lumpy
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

function getSketchyArrow(fromX, fromY, toX, toY, roughness, seed, arrowLength) {
    if (roughness === 0) return [];
    let rand = createPRNG(seed);
    let strokes = [];

    // The stem and both head strokes meet at one tip.
    let tip = _jitterPoint(toX, toY, roughness, rand);
    let from = _jitterPoint(fromX, fromY, roughness, rand);
    strokes = strokes.concat(getSketchyLineWithPRNG(from.x, from.y, tip.x, tip.y, roughness, rand, false, true));
    toX = tip.x;
    toY = tip.y;

    let lineAngle = Math.atan2(toY - fromY, toX - fromX);
    let len = arrowLength === undefined ? 12.0 : arrowLength;
    let arrowHalfAngle = Math.PI / 6.0;

    let arrowLeftX = toX - len * Math.cos(lineAngle - arrowHalfAngle);
    let arrowLeftY = toY - len * Math.sin(lineAngle - arrowHalfAngle);

    let arrowRightX = toX - len * Math.cos(lineAngle + arrowHalfAngle);
    let arrowRightY = toY - len * Math.sin(lineAngle + arrowHalfAngle);

    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowLeftX, arrowLeftY, roughness, rand, false, true));
    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowRightX, arrowRightY, roughness, rand, false, true));

    return strokes;
}

function getSketchyFreehand(points, roughness, seed) {
    if (!points || points.length < 2 || roughness === 0) return [];

    // The hand already makes freehand lines uneven; jittering every sample made them shaky.
    return [evaluateCatmullRomSpline(points, 8)];
}

function pointDistSq(a, b) {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    return dx * dx + dy * dy;
}

function copyPoint(p) {
    return Qt.point(p.x, p.y);
}

function copyPoints(points) {
    let out = [];
    for (let i = 0; i < points.length; ++i) {
        out.push(copyPoint(points[i]));
    }
    return out;
}

// Drop consecutive samples closer than sqrt(minDistSq).
function dedupPoints(points, minDistSq) {
    if (!points || points.length === 0) return [];
    let out = [copyPoint(points[0])];
    for (let i = 1; i < points.length; ++i) {
        if (pointDistSq(out[out.length - 1], points[i]) >= minDistSq) {
            out.push(copyPoint(points[i]));
        }
    }
    // Always keep the true endpoint if it was collapsed away
    let last = points[points.length - 1];
    if (out.length === 1 || pointDistSq(out[out.length - 1], last) > 1e-12) {
        if (pointDistSq(out[out.length - 1], last) >= minDistSq || out.length === 1) {
            // if last equals only point, still fine; if collapsed, force endpoint
            if (pointDistSq(out[out.length - 1], last) > 1e-12) {
                out.push(copyPoint(last));
            }
        } else {
            out[out.length - 1] = copyPoint(last);
        }
    }
    return out;
}

function rdpPerpendicularDistSq(p, a, b) {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-12) {
        return pointDistSq(p, a);
    }
    // Area formula: |(p-a) × (b-a)| / |b-a|  → squared
    let cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
    return (cross * cross) / lenSq;
}

// Classic Ramer–Douglas–Peucker. points length >= 2. epsilon in pixels.
function rdpSimplify(points, epsilon) {
    if (!points || points.length < 3) {
        return copyPoints(points || []);
    }
    let epsSq = epsilon * epsilon;
    let keep = [];
    for (let i = 0; i < points.length; ++i) {
        keep.push(false);
    }
    keep[0] = true;
    keep[points.length - 1] = true;

    let stack = [[0, points.length - 1]];
    while (stack.length > 0) {
        let seg = stack.pop();
        let start = seg[0];
        let end = seg[1];
        let maxDist = -1;
        let index = -1;
        for (let i = start + 1; i < end; ++i) {
            let d = rdpPerpendicularDistSq(points[i], points[start], points[end]);
            if (d > maxDist) {
                maxDist = d;
                index = i;
            }
        }
        if (index >= 0 && maxDist > epsSq) {
            keep[index] = true;
            stack.push([start, index]);
            stack.push([index, end]);
        }
    }

    let out = [];
    for (let i = 0; i < points.length; ++i) {
        if (keep[i]) {
            out.push(copyPoint(points[i]));
        }
    }
    return out;
}

// Chaikin corner-cutting on an open polyline. Endpoints stay fixed.
// One iteration: for each segment (P_i, P_{i+1}) emit Q=0.75P_i+0.25P_{i+1}, R=0.25P_i+0.75P_{i+1}
// then [P0, Q0, R0, Q1, R1, ..., Pn].
function chaikinSmooth(points, iterations) {
    if (!points || points.length < 3 || iterations <= 0) {
        return copyPoints(points || []);
    }
    let pts = copyPoints(points);
    for (let iter = 0; iter < iterations; ++iter) {
        if (pts.length < 3) break;
        let next = [];
        next.push(copyPoint(pts[0]));
        for (let i = 0; i < pts.length - 1; ++i) {
            let p0 = pts[i];
            let p1 = pts[i + 1];
            next.push(Qt.point(
                0.75 * p0.x + 0.25 * p1.x,
                0.75 * p0.y + 0.25 * p1.y
            ));
            next.push(Qt.point(
                0.25 * p0.x + 0.75 * p1.x,
                0.25 * p0.y + 0.75 * p1.y
            ));
        }
        next.push(copyPoint(pts[pts.length - 1]));
        pts = next;
    }
    return pts;
}

function smoothFreehandLevelParams(level) {
    let lv = level | 0;
    if (lv < 0) lv = 0;
    if (lv > 3) lv = 3;
    // epsilon px, chaikin iterations
    if (lv === 0) return { epsilon: 0, iterations: 0 };
    if (lv === 1) return { epsilon: 1.5, iterations: 0 };
    if (lv === 2) return { epsilon: 2.5, iterations: 1 };
    return { epsilon: 4.0, iterations: 2 };
}

// Public entry: smooth freehand samples for commit.
// points: array of Qt.point / {x,y}. level: 0..3.
function smoothFreehandPoints(points, level) {
    if (!points || points.length === 0) return [];
    if (points.length < 2) return copyPoints(points);

    let params = smoothFreehandLevelParams(level);
    if (params.epsilon === 0 && params.iterations === 0) {
        return copyPoints(points);
    }

    let first = copyPoint(points[0]);
    let last = copyPoint(points[points.length - 1]);

    // 0.5px consecutive dedup
    let pts = dedupPoints(points, 0.25);
    if (pts.length < 3) {
        return [first, last];
    }

    pts = rdpSimplify(pts, params.epsilon);
    if (pts.length < 2) {
        return [first, last];
    }

    pts = chaikinSmooth(pts, params.iterations);

    // Pin endpoints to the original stroke ends
    pts[0] = first;
    pts[pts.length - 1] = last;
    return pts;
}
