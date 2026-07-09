.pragma library

function createPRNG(seed) {
    let s = seed;
    return function() {
        s = (s * 9301 + 49297) % 233280;
        return s / 233280.0;
    };
}

function getSketchyLine(x1, y1, x2, y2, roughness, seed) {
    let rand = createPRNG(seed);
    return getSketchyLineWithPRNG(x1, y1, x2, y2, roughness, rand);
}

function getSketchyLineWithPRNG(x1, y1, x2, y2, roughness, rand) {
    if (roughness === 0) {
        return [[Qt.point(x1, y1), Qt.point(x2, y2)]];
    }
    
    let L = Math.sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    if (L < 2) return [];
    
    let strokes = [];
    let roughnessFactor = roughness === 1 ? 3.5 : 7.0;
    
    for (let i = 0; i < 2; ++i) {
        let offset = roughnessFactor * (L > 100 ? 1.0 : Math.max(0.3, L / 100.0)) * (0.6 + rand() * 0.8);
        
        let dirX = (x2 - x1) / L;
        let dirY = (y2 - y1) / L;
        let perpX = -dirY;
        let perpY = dirX;
        
        let os1 = (rand() - 0.5) * 5.0 * (roughnessFactor / 3.0);
        let os2 = (rand() - 0.5) * 5.0 * (roughnessFactor / 3.0);
        
        let sx = x1 - dirX * os1 + perpX * (rand() - 0.5) * offset;
        let sy = y1 - dirY * os1 + perpY * (rand() - 0.5) * offset;
        let ex = x2 + dirX * os2 + perpX * (rand() - 0.5) * offset;
        let ey = y2 + dirY * os2 + perpY * (rand() - 0.5) * offset;
        
        let mx = (sx + ex) / 2 + perpX * (rand() - 0.5) * offset * 2.0;
        let my = (sy + ey) / 2 + perpY * (rand() - 0.5) * offset * 2.0;
        
        strokes.push([Qt.point(sx, sy), Qt.point(mx, my), Qt.point(ex, ey)]);
    }
    return strokes;
}

function getSketchyArc(cx, cy, R, startAngle, endAngle, roughness, rand) {
    if (R <= 0) return [];
    let strokes = [];
    let roughnessFactor = roughness === 1 ? 3.5 : 7.0;
    
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
    let R = borderRadius || 0;
    if (R <= 0) {
        if (roughness === 0) {
            return [[Qt.point(x, y), Qt.point(x + w, y), Qt.point(x + w, y + h), Qt.point(x, y + h), Qt.point(x, y)]];
        }
        let rand = createPRNG(seed);
        let strokes = [];
        strokes = strokes.concat(getSketchyLineWithPRNG(x, y, x + w, y, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x + w, y, x + w, y + h, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x + w, y + h, x, y + h, roughness, rand));
        strokes = strokes.concat(getSketchyLineWithPRNG(x, y + h, x, y, roughness, rand));
        return strokes;
    }
    
    if (roughness === 0) {
        let pts = [];
        let steps = 8;
        pts.push(Qt.point(x + R, y));
        pts.push(Qt.point(x + w - R, y));
        for (let i = 0; i <= steps; ++i) {
            let theta = -Math.PI / 2 + (i / steps) * (Math.PI / 2);
            pts.push(Qt.point(x + w - R + R * Math.cos(theta), y + R + R * Math.sin(theta)));
        }
        pts.push(Qt.point(x + w, y + h - R));
        for (let i = 0; i <= steps; ++i) {
            let theta = 0 + (i / steps) * (Math.PI / 2);
            pts.push(Qt.point(x + w - R + R * Math.cos(theta), y + h - R + R * Math.sin(theta)));
        }
        pts.push(Qt.point(x + R, y + h));
        for (let i = 0; i <= steps; ++i) {
            let theta = Math.PI / 2 + (i / steps) * (Math.PI / 2);
            pts.push(Qt.point(x + R + R * Math.cos(theta), y + h - R + R * Math.sin(theta)));
        }
        pts.push(Qt.point(x, y + R));
        for (let i = 0; i <= steps; ++i) {
            let theta = Math.PI + (i / steps) * (Math.PI / 2);
            pts.push(Qt.point(x + R + R * Math.cos(theta), y + R + R * Math.sin(theta)));
        }
        return [pts];
    }
    
    let rand = createPRNG(seed);
    let strokes = [];
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
    let cx = x + w / 2;
    let cy = y + h / 2;
    let rx = w / 2;
    let ry = h / 2;
    if (rx < 1 || ry < 1) return [];
    
    if (roughness === 0) {
        let pts = [];
        let steps = 64;
        for (let i = 0; i <= steps; ++i) {
            let theta = (i / steps) * 2 * Math.PI;
            pts.push(Qt.point(cx + rx * Math.cos(theta), cy + ry * Math.sin(theta)));
        }
        return [pts];
    }
    
    let rand = createPRNG(seed);
    let strokes = [];
    let roughnessFactor = roughness === 1 ? 3.5 : 7.0;
    
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

function getSketchyArrow(fromX, fromY, toX, toY, roughness, seed) {
    let rand = createPRNG(seed);
    let strokes = [];
    
    strokes = strokes.concat(getSketchyLineWithPRNG(fromX, fromY, toX, toY, roughness, rand));
    
    let lineAngle = Math.atan2(toY - fromY, toX - fromX);
    let arrowLength = 12;
    let arrowHalfAngle = Math.PI / 6;
    
    let arrowLeftX = toX - arrowLength * Math.cos(lineAngle - arrowHalfAngle);
    let arrowLeftY = toY - arrowLength * Math.sin(lineAngle - arrowHalfAngle);
    
    let arrowRightX = toX - arrowLength * Math.cos(lineAngle + arrowHalfAngle);
    let arrowRightY = toY - arrowLength * Math.sin(lineAngle + arrowHalfAngle);
    
    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowLeftX, arrowLeftY, roughness, rand));
    strokes = strokes.concat(getSketchyLineWithPRNG(toX, toY, arrowRightX, arrowRightY, roughness, rand));
    
    return strokes;
}

function getSketchyFreehand(points, roughness, seed) {
    if (!points || points.length < 2) return [];
    if (roughness === 0) {
        let pts = [];
        for (let i = 0; i < points.length; ++i) {
            pts.push(Qt.point(points[i].x, points[i].y));
        }
        return [pts];
    }
    
    let rand = createPRNG(seed);
    let strokes = [];
    let roughnessFactor = roughness === 1 ? 2.5 : 5.0;
    
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
