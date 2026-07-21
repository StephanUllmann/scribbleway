# Save / Load Sessions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist the overlay shape list across daemon restarts so annotations survive quit/crash/reboot, written to `~/.local/share/scribbleway/session.json`.

**Architecture:** Keep a native Scribbleway session JSON (not Excalidraw clipboard). `ShapesModel` owns pure serialize/deserialize of `m_shapes`. `OverlayController` owns file path, atomic write, debounced autosave, explicit save, and load. `main.cpp` loads once at startup before QML binds the model. Optional local hotkey `Ctrl+S` forces an immediate save.

**Tech Stack:** Qt 6 (QJson*, QSaveFile, QStandardPaths, QTimer), C++20, existing QVariantMap shape dicts, Qt Test.

## Global Constraints

* **Path:** `QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/scribbleway/session.json"` → `~/.local/share/scribbleway/session.json` on Linux. Do **not** use `AppDataLocation` here: with `organizationDomain = kde.org` that nests under `kde.org/scribbleway`.
* **Format version:** root object `{"version": 1, "shapes": [ ... ]}`. Unknown future versions: refuse load (log + leave model empty/current).
* **What is persisted:** full shape property maps (type, geometry, style, locked, roughness, seed, glow, text, points, …).
* **What is not persisted:**
  * `selected` — always write `false` / strip on save; load with `selected: false`.
  * Undo history (`m_history`) — load replaces shapes and clears history.
  * Tool, mode, defaults, selection index, screen — session is shapes only.
* **Triggers:** both autosave (debounced) **and** explicit save hotkey.
* **Load timing:** once after `OverlayController` construction in `main.cpp`, before `engine.load`.
* **Safety:** atomic write via `QSaveFile`; missing/corrupt file = no-op (empty session), never crash.
* **No Excalidraw coupling:** session format is independent of `convertToExcalidraw` / clipboard.

## Session JSON Schema (v1)

```json
{
  "version": 1,
  "shapes": [
    {
      "type": "rectangle",
      "color": "#e63946",
      "strokeWidth": 2,
      "opacity": 1.0,
      "x": 10.0,
      "y": 20.0,
      "width": 100.0,
      "height": 50.0,
      "borderRadius": 8,
      "roughness": 1,
      "seed": 123456,
      "glow": 10,
      "locked": false,
      "selected": false
    },
    {
      "type": "freehand",
      "color": "#e63946",
      "strokeWidth": 2,
      "opacity": 1.0,
      "points": [ { "x": 0.0, "y": 0.0 }, { "x": 10.0, "y": 20.0 } ],
      "roughness": 1,
      "seed": 42,
      "glow": 3,
      "locked": false,
      "selected": false
    },
    {
      "type": "line",
      "fromX": 0, "fromY": 0, "toX": 50, "toY": 50,
      "...": "same style keys"
    }
  ]
}
```

**Points encoding:** freehand `points` must be JSON arrays of `{"x":number,"y":number}`. Do not rely on `QJsonValue::fromVariant(QPointF)` (user-type, not portable JSON). On load, reuse existing `normalizePoints()` path in `shapesmodel.cpp` (already accepts `QVariantMap` with `x`/`y`, lines 4–23).

## File Structure & Decomposition

* `src/overlay/shapesmodel.h` / `.cpp` — `toJsonArray()` / `loadFromJson(const QJsonArray &)` (replace + clear history, no undo snapshot).
* `src/overlay/overlaycontroller.h` / `.cpp` — `sessionFilePath()`, `saveSession()`, `loadSession()`, debounced `scheduleAutosave()`, `flushAutosave()`, hook into mutation paths + `aboutToQuit`.
* `src/overlay/main.cpp` — call `controller.loadSession()` after construct.
* `src/overlay/qml/main.qml` — optional `Ctrl+S` → `controller.saveSession()`.
* `tests/shapesmodeltest.cpp` — roundtrip + load edge cases.

---

### Task 1: ShapesModel JSON serialize / deserialize

**Files:**
* Modify: `src/overlay/shapesmodel.h`
* Modify: `src/overlay/shapesmodel.cpp`
* Test: `tests/shapesmodeltest.cpp` (compiled in Task 3)

**Interfaces:**
* Consumes: `m_shapes` (`QList<QVariantMap>`), existing `normalizePoints`, `setShapes` history flag pattern (`m_isApplyingUndo`).
* Produces: `QJsonArray ShapesModel::toJsonArray() const`, `void ShapesModel::loadFromJson(const QJsonArray &array)`.

- [ ] **Step 1: Declare API on ShapesModel**

In `src/overlay/shapesmodel.h` after `shapes()` (around lines 46–48):

```cpp
    QJsonArray toJsonArray() const;
    void loadFromJson(const QJsonArray &array);
```

Add includes:

```cpp
#include <QJsonArray>
#include <QJsonObject>
```

- [ ] **Step 2: Implement `toJsonArray()`**

In `shapesmodel.cpp`:

```cpp
static QJsonValue shapeToJson(const QVariantMap &shape)
{
    QJsonObject obj;
    for (auto it = shape.constBegin(); it != shape.constEnd(); ++it) {
        const QString &key = it.key();
        if (key == QStringLiteral("selected")) {
            obj.insert(key, false); // never persist selection
            continue;
        }
        if (key == QStringLiteral("points")) {
            QJsonArray pts;
            const QVariantList list = it.value().toList();
            for (const QVariant &v : list) {
                QPointF p;
                if (v.canConvert<QPointF>()) {
                    p = v.toPointF();
                } else if (v.typeId() == QMetaType::QVariantMap) {
                    const QVariantMap m = v.toMap();
                    p = QPointF(m.value(QStringLiteral("x")).toDouble(),
                                m.value(QStringLiteral("y")).toDouble());
                } else {
                    continue;
                }
                pts.append(QJsonObject{
                    {QStringLiteral("x"), p.x()},
                    {QStringLiteral("y"), p.y()}
                });
            }
            obj.insert(key, pts);
            continue;
        }
        // Portable JSON: bool/double/string/int via QJsonValue::fromVariant
        obj.insert(key, QJsonValue::fromVariant(it.value()));
    }
    if (!obj.contains(QStringLiteral("selected"))) {
        obj.insert(QStringLiteral("selected"), false);
    }
    return obj;
}

QJsonArray ShapesModel::toJsonArray() const
{
    QJsonArray arr;
    arr.reserve(m_shapes.size());
    for (const QVariantMap &shape : m_shapes) {
        arr.append(shapeToJson(shape));
    }
    return arr;
}
```

- [ ] **Step 3: Implement `loadFromJson()`**

Must:
1. Build `QList<QVariantMap>` from array objects.
2. Force `selected = false` on every shape.
3. Run `points` through `normalizePoints` when present (same as `addShape`).
4. Replace model **without** pushing undo history and **clear** `m_history`.

```cpp
void ShapesModel::loadFromJson(const QJsonArray &array)
{
    QList<QVariantMap> loaded;
    loaded.reserve(array.size());
    for (const QJsonValue &val : array) {
        if (!val.isObject()) {
            continue;
        }
        QVariantMap shape = val.toObject().toVariantMap();
        shape.insert(QStringLiteral("selected"), false);
        if (shape.contains(QStringLiteral("points"))) {
            shape.insert(QStringLiteral("points"), normalizePoints(shape.value(QStringLiteral("points"))));
        }
        // Skip entries with empty/unknown type? Prefer keep; renderer already ignores bad types.
        if (shape.value(QStringLiteral("type")).toString().isEmpty()) {
            continue;
        }
        loaded.append(shape);
    }

    m_isApplyingUndo = true; // suppresses saveHistorySnapshot inside setShapes
    setShapes(loaded);
    m_isApplyingUndo = false;
    m_history.clear();
}
```

Note: `setShapes` already `beginResetModel` / assign / `endResetModel` (lines 131–139). Reusing it avoids a second reset path.

- [ ] **Step 4: Verify compile**

```bash
cmake --build build --target scribbleway-overlay
```

Expected: success.

- [ ] **Step 5: Commit**

```bash
git add src/overlay/shapesmodel.h src/overlay/shapesmodel.cpp
git commit -m "feat(shapes): add session JSON serialize/deserialize"
```

---

### Task 2: OverlayController save / load / autosave

**Files:**
* Modify: `src/overlay/overlaycontroller.h`
* Modify: `src/overlay/overlaycontroller.cpp`
* Modify: `src/overlay/main.cpp`

**Interfaces:**
* Consumes: `ShapesModel::toJsonArray` / `loadFromJson`.
* Produces:
  * `Q_INVOKABLE bool saveSession()` (also D-Bus slot if under `public Q_SLOTS` — prefer `public Q_SLOTS` so applet can call later)
  * `Q_INVOKABLE bool loadSession()`
  * private `QString sessionFilePath() const`
  * private `void scheduleAutosave()`
  * private `void flushAutosave()` (timer slot)
  * members: `QTimer m_autosaveTimer`, `bool m_autosaveEnabled = true`, `bool m_loadingSession = false`

**Design decisions (implement exactly):**

| Concern | Choice |
|---|---|
| Autosave debounce | Single-shot `QTimer`, interval **400 ms** |
| When to schedule | `notifyShapesChanged()` **and** `OverlayController::endEdit()` (QML mouse-release after drag/resize). Do **not** hook `m_shapesModel.endEdit` inside `dragSelected` — that fires every move. |
| During load | `m_loadingSession = true` suppresses autosave |
| Explicit save | Cancels pending timer and writes immediately |
| Quit | Connect `qApp->aboutToQuit` → `flushAutosave()` / `saveSession()` if timer active |
| Write | `QDir().mkpath(parent)`, then `QSaveFile` + `commit()` |
| Read | `QFile` + `QJsonDocument::fromJson`; validate `version == 1` and `shapes` is array |

- [ ] **Step 1: Header declarations**

`overlaycontroller.h`:

Add includes if missing:

```cpp
#include <QTimer>
```

In `public Q_SLOTS:` (near `clear()`, ~line 123):

```cpp
    bool saveSession();
    bool loadSession();
```

In `private:` (near `notifyShapesChanged`, ~line 163):

```cpp
    QString sessionFilePath() const;
    void scheduleAutosave();
    void flushAutosave();
```

Members (~line 188):

```cpp
    QTimer m_autosaveTimer;
    bool m_loadingSession = false;
```

- [ ] **Step 2: Constructor wiring**

In `OverlayController::OverlayController` after shortcut load (~line 69):

```cpp
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(400);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &OverlayController::flushAutosave);

    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() {
            if (m_autosaveTimer.isActive()) {
                m_autosaveTimer.stop();
                saveSession();
            }
        });
    }
```

Includes: `<QCoreApplication>`, `<QStandardPaths>`, `<QDir>`, `<QFile>`, `<QSaveFile>`, `<QJsonDocument>` (document already present).

- [ ] **Step 3: Path + save + load implementation**

```cpp
QString OverlayController::sessionFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/scribbleway/session.json");
}

bool OverlayController::saveSession()
{
    if (m_loadingSession) {
        return false;
    }
    m_autosaveTimer.stop();

    const QString path = sessionFilePath();
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "saveSession: cannot create" << dir;
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("shapes"), m_shapesModel.toJsonArray());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "saveSession: open failed" << path << file.errorString();
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size()) {
        qWarning() << "saveSession: write failed" << path;
        return false;
    }
    if (!file.commit()) {
        qWarning() << "saveSession: commit failed" << path << file.errorString();
        return false;
    }
    return true;
}

bool OverlayController::loadSession()
{
    const QString path = sessionFilePath();
    QFile file(path);
    if (!file.exists()) {
        return true; // empty session is success
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "loadSession: open failed" << path << file.errorString();
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "loadSession: invalid JSON" << path << err.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) {
        qWarning() << "loadSession: unsupported version"
                   << root.value(QStringLiteral("version"));
        return false;
    }
    if (!root.value(QStringLiteral("shapes")).isArray()) {
        qWarning() << "loadSession: missing shapes array";
        return false;
    }

    m_loadingSession = true;
    m_autosaveTimer.stop();
    m_shapesModel.loadFromJson(root.value(QStringLiteral("shapes")).toArray());
    m_selectedIndex = -1;
    m_loadingSession = false;

    notifyShapesChanged();
    notifySelectionChanged();
    return true;
}

void OverlayController::scheduleAutosave()
{
    if (m_loadingSession) {
        return;
    }
    m_autosaveTimer.start(); // restarts debounce
}

void OverlayController::flushAutosave()
{
    saveSession();
}
```

- [ ] **Step 4: Hook schedule points**

1. `notifyShapesChanged()` (~line 693):

```cpp
void OverlayController::notifyShapesChanged()
{
    Q_EMIT shapesMetadataChanged(shapesMetadata());
    scheduleAutosave();
}
```

Note: selection-only updates also call this (via `setSelectedIndex`). Debounce + `selected` stripped on write makes this cheap/harmless.

2. `OverlayController::endEdit()` (~line 484) — covers drag/resize mouse-release from QML (`BaseShape.qml` calls `controller.endEdit()` on release; mid-drag `dragSelected` uses `m_shapesModel.endEdit()` only and does **not** go through this):

```cpp
void OverlayController::endEdit()
{
    m_shapesModel.endEdit();
    scheduleAutosave();
}
```

Do **not** add autosave inside `dragSelected` (lines 1438–1478).

- [ ] **Step 5: Load on startup in main.cpp**

After `OverlayController controller;` (~line 34), before D-Bus register is fine; must be before `engine.load`:

```cpp
    OverlayController controller;
    controller.loadSession();
```

No QML change required for load — `ShapesModel` reset notifies the Repeater when the engine binds.

- [ ] **Step 6: Verify compile**

```bash
cmake --build build --target scribbleway-overlay
```

- [ ] **Step 7: Commit**

```bash
git add src/overlay/overlaycontroller.h src/overlay/overlaycontroller.cpp src/overlay/main.cpp
git commit -m "feat(overlay): persist session to ~/.local/share/scribbleway/session.json"
```

---

### Task 3: Explicit save hotkey (optional but in scope)

**Files:**
* Modify: `src/overlay/overlaycontroller.cpp` (local shortcut table)
* Modify: `src/overlay/qml/main.qml`

**Pattern:** same as `action_undo` / `action_clear` (controller lines 57–58, QML lines 664–672).

- [ ] **Step 1: Register local shortcut default**

In constructor `m_localShortcuts` list, after `action_clear`:

```cpp
        {QStringLiteral("action_save"), QStringLiteral("Save Session"), QStringLiteral("Ctrl+S"), QStringLiteral("Ctrl+S")},
```

No `main.cpp` legacy cleanup needed (never was global).

- [ ] **Step 2: QML Shortcut**

In `src/overlay/qml/main.qml` near the undo/clear shortcuts (~line 664):

```qml
    Shortcut {
        sequence: controller.localShortcutSequences["action_save"]
        enabled: canvasWindow.shortcutGuard
        onActivated: controller.saveSession()
    }
```

Also works via `StandardKey.Save` if preferred, but stay consistent with the local-shortcut table so the applet shortcut editor can rebind it.

- [ ] **Step 3: Commit**

```bash
git add src/overlay/overlaycontroller.cpp src/overlay/qml/main.qml
git commit -m "feat(overlay): Ctrl+S explicit session save shortcut"
```

---

### Task 4: Tests

**Files:**
* Modify: `tests/shapesmodeltest.cpp`

**Includes to add if missing:** `<QJsonArray>`, `<QTemporaryDir>`, `<QStandardPaths>`, `<QDir>`, `<QFile>`, `<QSignalSpy>` (spy already included).

- [ ] **Step 1: Declare test slots**

In `ShapesModelTest` private slots (~line 50):

```cpp
    void testSessionRoundtrip();
    void testSessionLoadMissingAndCorrupt();
    void testSessionLoadClearsSelectionAndHistory();
```

- [ ] **Step 2: `testSessionRoundtrip` — model-level all types**

Cover rectangle, ellipse, text, line, arrow, freehand (with `QPointF` points). Assert:

* `toJsonArray` size matches.
* Freehand points in JSON are objects with `x`/`y`.
* `selected` in JSON is always `false` even if source had `true`.
* After `loadFromJson`, geometry/style/locked/roughness/seed/glow/text/font*/borderRadius/from*/to*/points match.
* Points after load are `QPointF`-convertible (via `normalizePoints`).

Sketch:

```cpp
void ShapesModelTest::testSessionRoundtrip()
{
    ShapesModel model;

    QVariantMap rect{
        {QStringLiteral("type"), QStringLiteral("rectangle")},
        {QStringLiteral("color"), QStringLiteral("#e63946")},
        {QStringLiteral("strokeWidth"), 3},
        {QStringLiteral("opacity"), 0.8},
        {QStringLiteral("x"), 10.0},
        {QStringLiteral("y"), 20.0},
        {QStringLiteral("width"), 100.0},
        {QStringLiteral("height"), 50.0},
        {QStringLiteral("borderRadius"), 12},
        {QStringLiteral("roughness"), 2},
        {QStringLiteral("seed"), 99},
        {QStringLiteral("glow"), 5},
        {QStringLiteral("locked"), true},
        {QStringLiteral("selected"), true}
    };
    model.addShape(rect);

    QVariantMap freehand{
        {QStringLiteral("type"), QStringLiteral("freehand")},
        {QStringLiteral("color"), QStringLiteral("#457b9d")},
        {QStringLiteral("strokeWidth"), 2},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("roughness"), 1},
        {QStringLiteral("seed"), 7},
        {QStringLiteral("glow"), 0},
        {QStringLiteral("locked"), false},
        {QStringLiteral("selected"), true},
        {QStringLiteral("points"), QVariantList{
            QVariant::fromValue(QPointF(1.0, 2.0)),
            QVariant::fromValue(QPointF(3.0, 4.0))
        }}
    };
    model.addShape(freehand);

    // + minimal line/arrow/ellipse/text similarly…

    const QJsonArray json = model.toJsonArray();
    QCOMPARE(json.size(), model.rowCount());
    QCOMPARE(json.at(0).toObject().value(QStringLiteral("selected")).toBool(), false);
    QVERIFY(json.at(/*freehand idx*/).toObject().value(QStringLiteral("points")).isArray());
    QCOMPARE(json.at(/*freehand*/).toObject().value(QStringLiteral("points")).toArray().at(0)
                 .toObject().value(QStringLiteral("x")).toDouble(), 1.0);

    ShapesModel loaded;
    loaded.addShape(QVariantMap{{QStringLiteral("type"), QStringLiteral("rectangle")}}); // noise
    loaded.loadFromJson(json);
    QCOMPARE(loaded.rowCount(), model.rowCount());
    QCOMPARE(loaded.shapes().at(0).value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(loaded.shapes().at(0).value(QStringLiteral("borderRadius")).toInt(), 12);
    QCOMPARE(loaded.shapes().at(0).value(QStringLiteral("locked")).toBool(), true);
    const QVariantList pts = loaded.shapes().at(/*freehand*/).value(QStringLiteral("points")).toList();
    QCOMPARE(pts.size(), 2);
    QCOMPARE(pts.at(0).toPointF(), QPointF(1.0, 2.0));

    // Undo must be a no-op (history cleared, no snapshot from load)
    loaded.undo();
    QCOMPARE(loaded.rowCount(), model.rowCount());
}
```

- [ ] **Step 3: `testSessionLoadMissingAndCorrupt` — controller file I/O**

Use a private override of the path **or** (preferred, less invasive) temporarily point env:

Qt honors `XDG_DATA_HOME` for `GenericDataLocation` on Linux. In the test:

```cpp
void ShapesModelTest::testSessionLoadMissingAndCorrupt()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    OverlayController controller;
    // missing file
    QVERIFY(controller.loadSession());
    QCOMPARE(controller.shapesModel()->rowCount(), 0);

    // corrupt file
    const QString path = tmp.path() + QStringLiteral("/scribbleway/session.json");
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{not-json");
    }
    QVERIFY(!controller.loadSession());
    QCOMPARE(controller.shapesModel()->rowCount(), 0);

    // good roundtrip via controller
    controller.addShape(QVariantMap{
        {QStringLiteral("type"), QStringLiteral("ellipse")},
        {QStringLiteral("x"), 5.0},
        {QStringLiteral("y"), 6.0},
        {QStringLiteral("width"), 30.0},
        {QStringLiteral("height"), 40.0},
        {QStringLiteral("color"), QStringLiteral("#2a9d8f")},
        {QStringLiteral("strokeWidth"), 2},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("selected"), true}
    });
    QVERIFY(controller.saveSession());
    QVERIFY(QFile::exists(path));

    OverlayController controller2;
    QVERIFY(controller2.loadSession());
    QCOMPARE(controller2.shapesModel()->rowCount(), 1);
    QCOMPARE(controller2.shapesModel()->shapes().at(0).value(QStringLiteral("type")).toString(),
             QStringLiteral("ellipse"));
    QCOMPARE(controller2.shapesModel()->shapes().at(0).value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(controller2.selectedIndex(), -1);
}
```

**Caveat:** if other tests construct `OverlayController` and might race on the same env var, restore previous `XDG_DATA_HOME` in the test teardown of this function (`qputenv` old value or `qunsetenv`). Prefer RAII:

```cpp
struct EnvRestorer {
    QByteArray key, old;
    bool had;
    EnvRestorer(const char *k, const QByteArray &v)
        : key(k), old(qgetenv(k)), had(qEnvironmentVariableIsSet(k)) {
        qputenv(k, v);
    }
    ~EnvRestorer() {
        if (had) qputenv(key, old); else qunsetenv(key);
    }
};
```

- [ ] **Step 4: `testSessionLoadClearsSelectionAndHistory`**

```cpp
void ShapesModelTest::testSessionLoadClearsSelectionAndHistory()
{
    ShapesModel model;
    model.addShape({{QStringLiteral("type"), QStringLiteral("rectangle")},
                    {QStringLiteral("x"), 1}, {QStringLiteral("selected"), true}});
    model.addShape({{QStringLiteral("type"), QStringLiteral("line")},
                    {QStringLiteral("fromX"), 0}, {QStringLiteral("fromY"), 0},
                    {QStringLiteral("toX"), 1}, {QStringLiteral("toY"), 1}});
    QCOMPARE(model.rowCount(), 2);

    const QJsonArray snap = model.toJsonArray();
    model.removeShape(1); // creates history
    QCOMPARE(model.rowCount(), 1);

    model.loadFromJson(snap);
    QCOMPARE(model.rowCount(), 2);
    model.undo(); // must not revert to 1-shape state
    QCOMPARE(model.rowCount(), 2);
    for (const auto &s : model.shapes()) {
        QCOMPARE(s.value(QStringLiteral("selected")).toBool(), false);
    }
}
```

- [ ] **Step 5: Run tests**

```bash
cmake --build build --target shapesmodeltest && ctest --test-dir build -R shapesmodeltest --output-on-failure
```

Expected: all slots pass, including the three new ones.

- [ ] **Step 6: Commit**

```bash
git add tests/shapesmodeltest.cpp
git commit -m "test: session save/load roundtrip and edge cases"
```

---

## Excalidraw Compatibility Impact

**None.** Session persistence uses a private Scribbleway schema. Clipboard copy/paste (`convertToExcalidraw` / `convertFromExcalidraw` in `overlaycontroller.cpp` ~1100–1324) is unchanged. Do not route session I/O through Excalidraw converters (they drop Scribbleway-only fidelity edges and remap freehand coordinates to relative form).

## Manual Smoke Checklist

1. Start overlay, draw rect + freehand + text, quit daemon (`pkill scribbleway` / stop service).
2. Confirm `~/.local/share/scribbleway/session.json` exists and contains `"version": 1` and shapes.
3. Restart daemon — shapes reappear in same places; none selected.
4. Drag a shape, wait ~0.5s, kill -9 daemon — file should still have last debounced positions if timer fired; for hard kill mid-debounce positions may lag one gesture (acceptable). `aboutToQuit` covers graceful exit.
5. Press `Ctrl+S` in select mode — file mtime updates immediately.
6. Clear all (`Ctrl+Delete`) — after debounce, file has `"shapes":[]`; restart → empty canvas.
7. Corrupt the JSON by hand → restart logs warning, empty canvas, no crash.

## Acceptance Criteria

* [ ] Daemon restart restores all shape types (rectangle, ellipse, line, arrow, freehand, text) with geometry, color, strokeWidth, opacity, locked, borderRadius, font*, roughness, seed, glow.
* [ ] Freehand points survive as absolute coordinates (not Excalidraw-relative).
* [ ] No shape is selected after load; `selectedIndex == -1`.
* [ ] Undo stack does not contain a “pre-load” snapshot; first undo after load is a no-op until a new edit.
* [ ] File path is `~/.local/share/scribbleway/session.json` (GenericDataLocation).
* [ ] Writes are atomic (`QSaveFile`); missing/corrupt/unsupported-version files do not crash.
* [ ] Autosave runs debounced after structural edits and after drag/resize `endEdit`; not every drag pixel.
* [ ] Explicit `Ctrl+S` (`action_save`) forces immediate save and is rebindable via existing local shortcut machinery.
* [ ] `main.cpp` loads session before QML engine load.
* [ ] Unit tests cover model roundtrip, controller file I/O with temp `XDG_DATA_HOME`, and history/selection clearing.
* [ ] Excalidraw clipboard tests still pass unchanged.

## Out of Scope

* Multiple named sessions / session picker UI.
* Persisting tool, mode, defaults, or multi-monitor target screen.
* Applet button for save/load (D-Bus slots are available if added under `public Q_SLOTS`; UI can come later).
* Migrating old formats (v1 is first).
* Cloud/sync.
