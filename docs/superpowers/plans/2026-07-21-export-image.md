# Export as PNG / SVG Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users export the current overlay annotations to a PNG (pixel-perfect grab of the rendered overlay) or SVG (vector reconstruction from the shapes model), via a local hotkey and an applet button.

**Architecture:**
- **PNG path:** Temporarily hide selection chrome, call `QQuickWindow::grabWindow()`, restore chrome, write `QImage` via `QImage::save()`.
- **SVG path:** Walk `ShapesModel::shapes()` and emit a self-contained SVG document (no screen grab). Geometry/style come from the same `QVariantMap` keys used everywhere else.
- **Triggers:** Local shortcut `action_export` (default `Ctrl+E`) in overlay QML; applet "Export" button → D-Bus `exportImage(format)`.
- **Path picker:** C++ `QFileDialog::getSaveFileName` inside the overlay process (daemon owns the window and filesystem write). Format inferred from extension, with an explicit `format` arg as fallback.

**Tech Stack:** Qt 6, C++20, QML Shortcuts, D-Bus (`org.kde.scribbleway.OverlayController`), Plasma applet QML.

## Global Constraints
* Do **not** capture the desktop under the overlay — only Scribbleway annotations (transparent window grab / pure vector).
* Hide selection borders, resize handles, and the marquee frame during PNG grab so they never appear in the file.
* Empty canvas → no file written; return `false` / log a warning.
* No new shape keys, ShapeRoles, or Excalidraw schema changes.
* PNG must include glow/roughness as currently rendered; SVG is a clean vector approximation (stroke paths, no MultiEffect glow rasterization).
* Default filenames: `scribbleway-YYYYMMDD-HHMMSS.png` / `.svg` under `QStandardPaths::PicturesLocation`.

## File Structure & Decomposition
* `src/overlay/overlaycontroller.h` / `.cpp` — `exportImage()`, `exportImageToPath()`, SVG builder, chrome-hide property.
* `src/overlay/qml/main.qml` — local Shortcut for export; optional `exporting` flag binding.
* `src/overlay/qml/shapes/BaseShape.qml` — gate selection chrome on `!controller.exportChromeHidden`.
* `src/applet-plugin/appletbackend.h` / `.cpp` — `exportImage(format)` invokable → D-Bus.
* `applet/contents/ui/FullRepresentation.qml` — Export button (optionally split PNG/SVG menu).
* `tests/shapesmodeltest.cpp` — unit tests for SVG generation and path-based PNG/SVG write (no interactive dialog).

---

### Task 1: Controller API + Chrome Hide Flag

**Files:**
* Modify: `src/overlay/overlaycontroller.h`
* Modify: `src/overlay/overlaycontroller.cpp`

**Interfaces:**
* Consumes: `m_window` (`QQuickWindow*`), `m_shapesModel.shapes()`.
* Produces:
  * `Q_PROPERTY(bool exportChromeHidden READ exportChromeHidden NOTIFY exportChromeHiddenChanged)`
  * `Q_INVOKABLE bool exportImage(const QString &format = QStringLiteral("png"))` — D-Bus slot + QML
  * `Q_INVOKABLE bool exportImageToPath(const QString &path, const QString &format = QString())` — tests / scripted path
  * Private: `QByteArray buildSvgDocument() const`, `QRectF shapesBoundingRect() const`, helpers to emit path data per type

- [ ] **Step 1: Declare API in `overlaycontroller.h`**

Around the existing Q_PROPERTY block (after `localShortcutSequences`, ~line 47):
```cpp
    Q_PROPERTY(bool exportChromeHidden READ exportChromeHidden NOTIFY exportChromeHiddenChanged)
```

In the Q_INVOKABLE section (~line 108):
```cpp
    Q_INVOKABLE bool exportImage(const QString &format = QStringLiteral("png"));
    Q_INVOKABLE bool exportImageToPath(const QString &path, const QString &format = QString());
    bool exportChromeHidden() const;
```

In Q_SIGNALS (~line 155):
```cpp
    void exportChromeHiddenChanged();
    void exportCompleted(bool success, const QString &path);
```

In private: (~line 161)
```cpp
    QByteArray buildSvgDocument() const;
    QRectF shapesBoundingRect() const;
    static QString svgEscape(const QString &text);
    static QString colorWithOpacity(const QString &color, double opacity);

    bool m_exportChromeHidden = false;
```

Also add includes at top of `.h` if needed — keep implementation includes in `.cpp`:
```cpp
// cpp only: QFileDialog, QImage, QStandardPaths, QDateTime, QFile, QTextStream, QPainterPath (optional)
```

- [ ] **Step 2: Register local shortcut default**

In constructor `m_localShortcuts` initializer (`overlaycontroller.cpp` ~lines 39–61), append:
```cpp
{QStringLiteral("action_export"), QStringLiteral("Export Image"), QStringLiteral("Ctrl+E"), QStringLiteral("Ctrl+E")},
```

- [ ] **Step 3: Implement chrome flag + export entry points**

```cpp
bool OverlayController::exportChromeHidden() const
{
    return m_exportChromeHidden;
}

bool OverlayController::exportImage(const QString &format)
{
    const QString fmt = format.toLower();
    if (fmt != QStringLiteral("png") && fmt != QStringLiteral("svg")) {
        qWarning() << "exportImage: unsupported format" << format;
        return false;
    }
    if (m_shapesModel.rowCount() == 0) {
        qWarning() << "exportImage: nothing to export";
        Q_EMIT exportCompleted(false, {});
        return false;
    }

    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    const QString filter = (fmt == QStringLiteral("svg"))
        ? QStringLiteral("SVG (*.svg)")
        : QStringLiteral("PNG (*.png)");
    const QString defaultPath = pictures + QLatin1Char('/')
        + QStringLiteral("scribbleway-%1.%2").arg(stamp, fmt);

    // Parentless dialog is OK for a frameless layer-shell window; still pass m_window if non-null.
    const QString path = QFileDialog::getSaveFileName(
        /* parent widget */ nullptr,
        tr("Export annotations"),
        defaultPath,
        filter);

    if (path.isEmpty()) {
        Q_EMIT exportCompleted(false, {});
        return false;
    }
    return exportImageToPath(path, fmt);
}

bool OverlayController::exportImageToPath(const QString &path, const QString &format)
{
    if (path.isEmpty()) {
        return false;
    }
    if (m_shapesModel.rowCount() == 0) {
        Q_EMIT exportCompleted(false, path);
        return false;
    }

    QString fmt = format.toLower();
    if (fmt.isEmpty()) {
        if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
            fmt = QStringLiteral("svg");
        else
            fmt = QStringLiteral("png");
    }

    bool ok = false;
    if (fmt == QStringLiteral("svg")) {
        const QByteArray svg = buildSvgDocument();
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            ok = f.write(svg) == svg.size();
            f.close();
        }
    } else {
        if (!m_window) {
            qWarning() << "exportImageToPath: no window to grab";
            Q_EMIT exportCompleted(false, path);
            return false;
        }

        // Hide selection UI for a clean grab
        m_exportChromeHidden = true;
        Q_EMIT exportChromeHiddenChanged();
        // Let QML rebind visibility before grab
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        const QImage img = m_window->grabWindow();

        m_exportChromeHidden = false;
        Q_EMIT exportChromeHiddenChanged();

        if (img.isNull()) {
            qWarning() << "exportImageToPath: grabWindow failed";
            Q_EMIT exportCompleted(false, path);
            return false;
        }
        ok = img.save(path, "PNG");
    }

    Q_EMIT exportCompleted(ok, path);
    return ok;
}
```

Add includes in `overlaycontroller.cpp`:
```cpp
#include <QFileDialog>
#include <QImage>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QCoreApplication>
#include <QEventLoop>
```

**Note:** `exportImage` / `exportImageToPath` must be in `public Q_SLOTS` **or** remain `Q_INVOKABLE` — D-Bus registration uses `ExportAllSlots`. Prefer moving them into the existing `public Q_SLOTS:` block (after `selectShape`) so D-Bus exposes them automatically via `main.cpp` `ExportAllSlots`. Keep `Q_INVOKABLE` only if left outside slots; simplest: declare as slots:

```cpp
public Q_SLOTS:
    // ...existing...
    bool exportImage(const QString &format = QStringLiteral("png"));
    bool exportImageToPath(const QString &path, const QString &format = QString());
```

- [ ] **Step 4: Implement `shapesBoundingRect()`**

Reuse the same per-type bounds logic as `pasteFromClipboard` (~lines 1002–1043):
```cpp
QRectF OverlayController::shapesBoundingRect() const
{
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    bool any = false;

    const auto shapes = m_shapesModel.shapes();
    for (const QVariantMap &shape : shapes) {
        const QString type = shape.value(QStringLiteral("type")).toString();
        auto expand = [&](double x, double y) {
            any = true;
            minX = qMin(minX, x); maxX = qMax(maxX, x);
            minY = qMin(minY, y); maxY = qMax(maxY, y);
        };
        auto expandStroke = [&](double x, double y, double sw) {
            expand(x - sw, y - sw);
            expand(x + sw, y + sw);
        };
        const double sw = shape.value(QStringLiteral("strokeWidth"), 2).toDouble();
        const double glow = shape.value(QStringLiteral("glow"), 0).toDouble();
        const double pad = sw + glow;

        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")
            || type == QStringLiteral("text")) {
            const double x = shape.value(QStringLiteral("x")).toDouble();
            const double y = shape.value(QStringLiteral("y")).toDouble();
            const double w = shape.value(QStringLiteral("width")).toDouble();
            const double h = shape.value(QStringLiteral("height")).toDouble();
            expand(x - pad, y - pad);
            expand(x + w + pad, y + h + pad);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            expandStroke(shape.value(QStringLiteral("fromX")).toDouble(),
                         shape.value(QStringLiteral("fromY")).toDouble(), pad);
            expandStroke(shape.value(QStringLiteral("toX")).toDouble(),
                         shape.value(QStringLiteral("toY")).toDouble(), pad);
        } else if (type == QStringLiteral("freehand")) {
            for (const QVariant &pv : shape.value(QStringLiteral("points")).toList()) {
                double px = 0, py = 0;
                if (pv.canConvert<QPointF>()) {
                    const QPointF p = pv.toPointF();
                    px = p.x(); py = p.y();
                } else if (pv.typeId() == QMetaType::QVariantMap) {
                    const QVariantMap pm = pv.toMap();
                    px = pm.value(QStringLiteral("x")).toDouble();
                    py = pm.value(QStringLiteral("y")).toDouble();
                } else {
                    continue;
                }
                expandStroke(px, py, pad);
            }
        }
    }
    if (!any) {
        return {};
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}
```

- [ ] **Step 5: Implement `buildSvgDocument()`**

Emit a full SVG. Use screen/window size when available so coordinates match the overlay; otherwise fall back to padded bounding box with a viewBox transform.

```cpp
QString OverlayController::svgEscape(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&': out += QLatin1String("&amp;"); break;
        case '<': out += QLatin1String("&lt;"); break;
        case '>': out += QLatin1String("&gt;"); break;
        case '"': out += QLatin1String("&quot;"); break;
        case '\'': out += QLatin1String("&apos;"); break;
        default: out += c; break;
        }
    }
    return out;
}

QString OverlayController::colorWithOpacity(const QString &color, double opacity)
{
    const QColor c(color);
    if (!c.isValid()) {
        return QStringLiteral("rgba(0,0,0,%1)").arg(opacity);
    }
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(opacity, 0, 'f', 3);
}

QByteArray OverlayController::buildSvgDocument() const
{
    double width = m_window ? m_window->width() : 0;
    double height = m_window ? m_window->height() : 0;
    if (width <= 0 || height <= 0) {
        const QRectF b = shapesBoundingRect();
        width = qMax(1.0, b.right() + 8);
        height = qMax(1.0, b.bottom() + 8);
    }

    QString body;
    body += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">\n")
        .arg(width, 0, 'f', 2).arg(height, 0, 'f', 2);

    // Optional transparent background rect omitted — keep SVG transparent like the overlay.

    for (const QVariantMap &shape : m_shapesModel.shapes()) {
        const QString type = shape.value(QStringLiteral("type")).toString();
        const QString stroke = colorWithOpacity(
            shape.value(QStringLiteral("color"), QStringLiteral("#000000")).toString(),
            shape.value(QStringLiteral("opacity"), 1.0).toDouble());
        const double sw = shape.value(QStringLiteral("strokeWidth"), 2).toDouble();
        const QString common = QStringLiteral(
            "fill=\"none\" stroke=\"%1\" stroke-width=\"%2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"")
            .arg(stroke).arg(sw, 0, 'f', 2);

        if (type == QStringLiteral("rectangle")) {
            const double x = shape.value(QStringLiteral("x")).toDouble();
            const double y = shape.value(QStringLiteral("y")).toDouble();
            const double w = shape.value(QStringLiteral("width")).toDouble();
            const double h = shape.value(QStringLiteral("height")).toDouble();
            const double r = shape.value(QStringLiteral("borderRadius"), 0).toDouble();
            body += QStringLiteral("  <rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"%5\" ry=\"%5\" %6 />\n")
                .arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(w, 0, 'f', 2).arg(h, 0, 'f', 2)
                .arg(r, 0, 'f', 2).arg(common);
        } else if (type == QStringLiteral("ellipse")) {
            const double x = shape.value(QStringLiteral("x")).toDouble();
            const double y = shape.value(QStringLiteral("y")).toDouble();
            const double w = shape.value(QStringLiteral("width")).toDouble();
            const double h = shape.value(QStringLiteral("height")).toDouble();
            body += QStringLiteral("  <ellipse cx=\"%1\" cy=\"%2\" rx=\"%3\" ry=\"%4\" %5 />\n")
                .arg(x + w / 2.0, 0, 'f', 2).arg(y + h / 2.0, 0, 'f', 2)
                .arg(qAbs(w) / 2.0, 0, 'f', 2).arg(qAbs(h) / 2.0, 0, 'f', 2).arg(common);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            const double x1 = shape.value(QStringLiteral("fromX")).toDouble();
            const double y1 = shape.value(QStringLiteral("fromY")).toDouble();
            const double x2 = shape.value(QStringLiteral("toX")).toDouble();
            const double y2 = shape.value(QStringLiteral("toY")).toDouble();
            body += QStringLiteral("  <line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" %5 />\n")
                .arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2).arg(x2, 0, 'f', 2).arg(y2, 0, 'f', 2).arg(common);
            if (type == QStringLiteral("arrow")) {
                // Simple arrow head matching ArrowShape.qml trig (approximate fixed size from stroke)
                const double angle = std::atan2(y2 - y1, x2 - x1);
                const double headLen = 12.0 + sw * 2.0;
                const double a1 = angle + M_PI - 0.4;
                const double a2 = angle + M_PI + 0.4;
                const double ax1 = x2 + headLen * std::cos(a1);
                const double ay1 = y2 + headLen * std::sin(a1);
                const double ax2 = x2 + headLen * std::cos(a2);
                const double ay2 = y2 + headLen * std::sin(a2);
                body += QStringLiteral(
                    "  <polyline points=\"%1,%2 %3,%4 %5,%6\" %7 />\n")
                    .arg(ax1, 0, 'f', 2).arg(ay1, 0, 'f', 2)
                    .arg(x2, 0, 'f', 2).arg(y2, 0, 'f', 2)
                    .arg(ax2, 0, 'f', 2).arg(ay2, 0, 'f', 2)
                    .arg(common);
            }
        } else if (type == QStringLiteral("freehand")) {
            QString pointsAttr;
            bool first = true;
            for (const QVariant &pv : shape.value(QStringLiteral("points")).toList()) {
                double px = 0, py = 0;
                if (pv.canConvert<QPointF>()) {
                    const QPointF p = pv.toPointF();
                    px = p.x(); py = p.y();
                } else if (pv.typeId() == QMetaType::QVariantMap) {
                    const QVariantMap pm = pv.toMap();
                    px = pm.value(QStringLiteral("x")).toDouble();
                    py = pm.value(QStringLiteral("y")).toDouble();
                } else {
                    continue;
                }
                if (!first) pointsAttr += QLatin1Char(' ');
                first = false;
                pointsAttr += QStringLiteral("%1,%2").arg(px, 0, 'f', 2).arg(py, 0, 'f', 2);
            }
            if (!pointsAttr.isEmpty()) {
                body += QStringLiteral("  <polyline points=\"%1\" %2 />\n")
                    .arg(pointsAttr, common);
            }
        } else if (type == QStringLiteral("text")) {
            const double x = shape.value(QStringLiteral("x")).toDouble();
            const double y = shape.value(QStringLiteral("y")).toDouble();
            const int fontSize = shape.value(QStringLiteral("fontSize"), 20).toInt();
            const QString family = shape.value(QStringLiteral("fontFamily"),
                                               QStringLiteral("sans-serif")).toString();
            const QString text = svgEscape(shape.value(QStringLiteral("text")).toString());
            // SVG text baseline ≈ top-left box used by TextShape: use dominant-baseline hanging
            body += QStringLiteral(
                "  <text x=\"%1\" y=\"%2\" fill=\"%3\" stroke=\"none\" "
                "font-family=\"%4\" font-size=\"%5\" dominant-baseline=\"hanging\">%6</text>\n")
                .arg(x, 0, 'f', 2).arg(y, 0, 'f', 2)
                .arg(stroke)
                .arg(svgEscape(family)).arg(fontSize)
                .arg(text);
        }
    }

    body += QStringLiteral("</svg>\n");
    return body.toUtf8();
}
```

Add `#include <cmath>` if `std::atan2` / `M_PI` not already available. On some platforms define `_USE_MATH_DEFINES` or use `3.14159265358979323846` instead of `M_PI`.

- [ ] **Step 6: Build overlay target**

Run: `cmake --build build --target scribbleway-overlay`
Expected: compiles clean.

- [ ] **Step 7: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp
git commit -m "feat: add exportImage PNG/SVG API on OverlayController"
```

---

### Task 2: Hide Selection Chrome During PNG Grab (QML)

**Files:**
* Modify: `src/overlay/qml/shapes/BaseShape.qml`
* Modify: `src/overlay/qml/main.qml` (selection frame + shortcut only; export call)

**Interfaces:**
* Consumes: `controller.exportChromeHidden`
* Produces: Clean PNG without blue handles / marquee

- [ ] **Step 1: Gate BaseShape selection UI**

In `BaseShape.qml` ~line 137, change:
```qml
    Item {
        visible: isSelected && !isLocked && !(typeof canvasWindow !== "undefined" && canvasWindow.editingShapeIndex === shapeIndex)
```
to:
```qml
    Item {
        visible: isSelected
                 && !isLocked
                 && !(typeof canvasWindow !== "undefined" && canvasWindow.editingShapeIndex === shapeIndex)
                 && !(typeof controller !== "undefined" && controller.exportChromeHidden)
```

- [ ] **Step 2: Gate selection marquee in `main.qml`**

Around line 437 (`selectionFrameRect`):
```qml
    Rectangle {
        id: selectionFrameRect
        visible: isSelectingFrame && !controller.exportChromeHidden
```

Also hide live draw previews during export if any could be visible (optional safety):
```qml
// On each live-preview root that uses `visible: isDrawing && ...`, AND with:
// && !controller.exportChromeHidden
```
Only needed if export can fire mid-gesture; shortcutGuard already blocks text editor, but mid-draw is possible — AND-guard the main previews (rectangle/ellipse/line/arrow/freehand) the same way.

- [ ] **Step 3: Add export Shortcut**

After the existing shortcut block (~after `color_6`, before background MouseArea ~line 703):
```qml
    Shortcut {
        sequence: controller.localShortcutSequences["action_export"]
        enabled: canvasWindow.shortcutGuard
        onActivated: controller.exportImage("png")
    }
```

**Optional (nice UX):** bind Ctrl+Shift+E for SVG if you also register `action_export_svg`. Keep v1 to a single shortcut that defaults to PNG; applet can request SVG explicitly.

- [ ] **Step 4: Commit**

```bash
git add src/overlay/qml/shapes/BaseShape.qml src/overlay/qml/main.qml
git commit -m "feat: hide selection chrome on export and bind Ctrl+E"
```

---

### Task 3: Applet Backend + UI Button

**Files:**
* Modify: `src/applet-plugin/appletbackend.h`
* Modify: `src/applet-plugin/appletbackend.cpp`
* Modify: `applet/contents/ui/FullRepresentation.qml`

**Interfaces:**
* Consumes: D-Bus slot `exportImage(QString)`
* Produces: `AppletBackend::exportImage(format)` for QML

- [ ] **Step 1: Declare invokable**

In `appletbackend.h` near other actions (~line 70):
```cpp
    Q_INVOKABLE void exportImage(const QString &format = QStringLiteral("png"));
```

- [ ] **Step 2: Implement**

In `appletbackend.cpp` after `clear()` (~line 168):
```cpp
void AppletBackend::exportImage(const QString &format)
{
    sendDBus(QStringLiteral("exportImage"), {format});
}
```

`sendDBus` already uses `asyncCallWithArgumentList` — dialog runs in the overlay process, which is correct (applet must not block).

- [ ] **Step 3: Add Export control in FullRepresentation**

In the Global Actions row (~lines 138–155), extend to three buttons:
```qml
        // Row 1: Global Actions
        RowLayout {
            Layout.fillWidth: true

            PlasmaComponents.Button {
                icon.name: "edit-undo"
                text: "Undo"
                Layout.fillWidth: true
                onClicked: root.backend.undo()
            }

            PlasmaComponents.Button {
                icon.name: "edit-clear"
                text: "Clear All"
                Layout.fillWidth: true
                onClicked: root.backend.clear()
            }

            PlasmaComponents.Button {
                icon.name: "document-export"
                text: "Export"
                Layout.fillWidth: true
                enabled: root.backend.overlayConnected
                onClicked: exportMenu.popup()

                PlasmaComponents.Menu {
                    id: exportMenu
                    PlasmaComponents.MenuItem {
                        text: "Export PNG…"
                        icon.name: "image-png"
                        onTriggered: root.backend.exportImage("png")
                    }
                    PlasmaComponents.MenuItem {
                        text: "Export SVG…"
                        icon.name: "image-svg+xml"
                        onTriggered: root.backend.exportImage("svg")
                    }
                }
            }
        }
```

If `PlasmaComponents.Menu` is awkward in the plasmoid style, use two compact buttons ("PNG" / "SVG") instead — same `exportImage` calls.

- [ ] **Step 4: Build plugin**

Run: `cmake --build build --target scribblewaybackend`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/applet-plugin/appletbackend.h src/applet-plugin/appletbackend.cpp applet/contents/ui/FullRepresentation.qml
git commit -m "feat: applet Export PNG/SVG button via D-Bus"
```

---

### Task 4: Tests

**Files:**
* Modify: `tests/shapesmodeltest.cpp`

**Interfaces:**
* Consumes: `exportImageToPath`, `buildSvgDocument` (indirectly via file output)
* Produces: `testExportImageSvg`, `testExportImagePngWithoutWindow`, `testExportImageEmpty`

- [ ] **Step 1: Declare slots**

In the private Q_SLOTS list (~line 50):
```cpp
    void testExportImageSvg();
    void testExportImageEmpty();
    void testExportImagePngRequiresWindow();
```

- [ ] **Step 2: Implement SVG export test**

```cpp
void ShapesModelTest::testExportImageSvg()
{
    OverlayController controller;

    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("x")] = 10;
    rect[QStringLiteral("y")] = 20;
    rect[QStringLiteral("width")] = 100;
    rect[QStringLiteral("height")] = 50;
    rect[QStringLiteral("color")] = QStringLiteral("#e63946");
    rect[QStringLiteral("strokeWidth")] = 3;
    rect[QStringLiteral("opacity")] = 0.5;
    rect[QStringLiteral("borderRadius")] = 8;
    rect[QStringLiteral("selected")] = false;
    rect[QStringLiteral("locked")] = false;
    controller.addShape(rect);

    QVariantMap line;
    line[QStringLiteral("type")] = QStringLiteral("line");
    line[QStringLiteral("fromX")] = 0;
    line[QStringLiteral("fromY")] = 0;
    line[QStringLiteral("toX")] = 40;
    line[QStringLiteral("toY")] = 40;
    line[QStringLiteral("color")] = QStringLiteral("#457b9d");
    line[QStringLiteral("strokeWidth")] = 2;
    line[QStringLiteral("opacity")] = 1.0;
    controller.addShape(line);

    QVariantMap text;
    text[QStringLiteral("type")] = QStringLiteral("text");
    text[QStringLiteral("x")] = 5;
    text[QStringLiteral("y")] = 5;
    text[QStringLiteral("width")] = 80;
    text[QStringLiteral("height")] = 24;
    text[QStringLiteral("text")] = QStringLiteral("Hello <world>");
    text[QStringLiteral("fontSize")] = 18;
    text[QStringLiteral("fontFamily")] = QStringLiteral("monospace");
    text[QStringLiteral("color")] = QStringLiteral("#2a9d8f");
    text[QStringLiteral("opacity")] = 1.0;
    controller.addShape(text);

    const QString path = QDir::temp().filePath(QStringLiteral("scribbleway-export-test.svg"));
    QVERIFY(controller.exportImageToPath(path, QStringLiteral("svg")));
    QVERIFY(QFile::exists(path));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray svg = f.readAll();
    f.close();
    QFile::remove(path);

    QVERIFY(svg.contains("<svg"));
    QVERIFY(svg.contains("<rect"));
    QVERIFY(svg.contains("<line"));
    QVERIFY(svg.contains("<text"));
    QVERIFY(svg.contains("Hello &lt;world&gt;")); // escaped
    QVERIFY(svg.contains("#e63946") || svg.contains("rgba(230,57,70"));
}

void ShapesModelTest::testExportImageEmpty()
{
    OverlayController controller;
    const QString path = QDir::temp().filePath(QStringLiteral("scribbleway-export-empty.svg"));
    QVERIFY(!controller.exportImageToPath(path, QStringLiteral("svg")));
    QVERIFY(!QFile::exists(path));
}

void ShapesModelTest::testExportImagePngRequiresWindow()
{
    OverlayController controller;
    QVariantMap rect;
    rect[QStringLiteral("type")] = QStringLiteral("rectangle");
    rect[QStringLiteral("x")] = 0;
    rect[QStringLiteral("y")] = 0;
    rect[QStringLiteral("width")] = 10;
    rect[QStringLiteral("height")] = 10;
    rect[QStringLiteral("color")] = QStringLiteral("#ffffff");
    rect[QStringLiteral("strokeWidth")] = 1;
    rect[QStringLiteral("opacity")] = 1.0;
    controller.addShape(rect);

    const QString path = QDir::temp().filePath(QStringLiteral("scribbleway-export-test.png"));
    // No QQuickWindow attached → PNG path must fail cleanly
    QVERIFY(!controller.exportImageToPath(path, QStringLiteral("png")));
    QVERIFY(!QFile::exists(path));
}
```

Add includes if missing:
```cpp
#include <QDir>
#include <QFile>
```

- [ ] **Step 3: Run tests**

```bash
cmake --build build --target scribbleway-tests && ctest --test-dir build -R ShapesModelTest --output-on-failure
```
Expected: all PASS, including the three new cases.

- [ ] **Step 4: Commit**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: cover SVG export and empty/PNG-without-window cases"
```

---

### Task 5: Manual Smoke Verification

- [ ] **Step 1: Run daemon + draw**

```bash
# from build tree or installed package
scribbleway-overlay &
# Meta+Shift+X → draw rectangle, ellipse, arrow, freehand, text with glow
```

- [ ] **Step 2: Hotkey PNG**

Press `Ctrl+E` in overlay (non-passthrough). Save dialog opens → pick `/tmp/sw-test.png` → open file:
* Annotations present
* No blue selection handles
* Transparent areas stay transparent (alpha)

- [ ] **Step 3: Applet SVG**

Open plasmoid → Export → Export SVG… → open in browser/Inkscape:
* Geometry roughly matches
* Text escapes correctly
* Arrow has head polyline

- [ ] **Step 4: Empty / cancel**

Clear all → Export → should no-op (warning in journal). Cancel dialog → no file, no crash.

- [ ] **Step 5: Final commit if smoke fixes needed**

---

## Excalidraw Compatibility Impact

**None.** Export is a one-way dump of the live model / framebuffer. No changes to `convertToExcalidraw` / `convertFromExcalidraw`, clipboard format, or shape keys. SVG is Scribbleway-native, not Excalidraw JSON.

Roughness: PNG captures the already-rough QML strokes via `grabWindow`. SVG uses clean geometric primitives (rect/ellipse/line/polyline) — acceptable; do **not** reimplement RoughPathGenerator in C++ for v1.

Glow: PNG includes MultiEffect glow. SVG omits glow (stroke only). Document this in the applet tooltip if desired:
```qml
Controls.ToolTip.text: "PNG captures rendered glow/roughness; SVG is clean vectors"
```

## Data Model / D-Bus Contract Summary

| Surface | Change |
|---|---|
| Shape `QVariantMap` keys | none |
| `ShapeRoles` | none |
| New Q_PROPERTY | `exportChromeHidden: bool` (transient UI flag) |
| New D-Bus slots | `bool exportImage(QString format)`, `bool exportImageToPath(QString path, QString format)` |
| New D-Bus signals | `exportCompleted(bool success, QString path)` (optional for applet toast; not required for v1 UI) |
| Local shortcut id | `action_export` default `Ctrl+E` |
| AppletBackend | `Q_INVOKABLE void exportImage(const QString &format = "png")` |

## Acceptance Criteria

1. With shapes on screen, `Ctrl+E` opens a save dialog and writes a PNG that shows annotations without selection chrome.
2. Applet **Export → PNG** and **Export → SVG** both produce valid files under the chosen path.
3. SVG contains elements for rectangle, ellipse, line, arrow (line+head), freehand polyline, and text; special characters in text are XML-escaped.
4. Empty canvas does not create a file and does not crash.
5. Canceling the dialog does not create a file and does not crash.
6. Existing tests remain green; new export tests pass.
7. No regressions to Excalidraw copy/paste or glow/roughness rendering.
8. D-Bus introspection on `org.kde.scribbleway.OverlayController` lists `exportImage`.

## Implementation Notes / Risks

* **Wayland grab:** `QQuickWindow::grabWindow()` is the Qt-supported path for FBO/window capture; if a compositor returns a null image, log and fail — do not fall back to desktop screenshot APIs (would leak other windows / break the “annotations only” contract).
* **Dialog parenting:** Overlay is LayerShell frameless; parentless `QFileDialog` is intentional. If focus issues appear on Plasma, set `Qt::Dialog` + transient parent via `QWidget::createWindowContainer` is overkill — try `QFileDialog` with `m_window` cast only if needed (QWindow-aware overload in Qt 6: `QFileDialog::getSaveFileName` is QWidget-based; use `QFileDialog` object + `dialog.setOption(QFileDialog::DontUseNativeDialog)` only as last resort).
* **Threading:** All export work stays on the GUI thread (required for grab + dialog).
* **Async D-Bus:** Applet must not expect a synchronous bool back through `sendDBus`; success UX can listen to `exportCompleted` later if desired (v1: fire-and-forget is fine).
* **ponytail:** Prefer one `exportImage(format)` over separate PNG/SVG methods; reuse paste bounds logic rather than inventing a second geometry walker if you can extract a private static helper — optional cleanup, not required for merge.
