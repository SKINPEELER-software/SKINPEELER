# Architecture

SKINPEELER is organized into three layers. Each layer only calls downward;
algorithmic logic lives in the engine layer, never in the UI layer.

```mermaid
flowchart TB
    subgraph UI["UI layer (src/views, cli)"]
        GUI["Qt 6 desktop GUI<br/>MainWindow / SkinPeelerScene / PlotWidgets / PlotWindow"]
        CLI["Batch command-line program<br/>skinpeeler_cli"]
    end
    subgraph ENGINE["SKINPEELER engine (src/models, src/services, src/engine)"]
        SE["Session flow<br/>skinpeeler::Session"]
        M["Models<br/>CTImage / CTShell / BedDetector / BedCache / ShellStatistics / BetaSelection"]
        S["Services<br/>DicomService / PeelingService / StatisticsService"]
    end
    subgraph LIB["V-ImageProcessing (third_party/v_imageprocessing)"]
        V["Voronoi diagram / beta-complex library<br/>Voronoi Diagram Research Center, Hanyang University"]
    end
    GUI --> S
    GUI --> M
    CLI --> SE
    CLI --> M
    SE --> S
    SE --> M
    S --> M
    M --> V
```

## Layers

1. **V-ImageProcessing** (`third_party/v_imageprocessing/`) — a prebuilt,
   proprietary computational geometry library (Voronoi diagram / beta-complex
   construction, exposed to this project through `ImageProcessingBC`). Not
   modified or redistributed in source form by this project; see
   `third_party/v_imageprocessing/NOTICE.md`.
2. **SKINPEELER engine** (`src/models/`, `src/services/`, `src/engine/`) —
   the algorithm implementation described in [`algorithm.md`](algorithm.md):
   DICOM I/O, quasi-triangulation/beta-complex orchestration, shell
   extraction, cutting-point statistics, masks, beta selection, and peeling.
   Has **no Qt dependency**, so it can be driven from a GUI, a CLI tool (see
   `cli/`), a test binary (see `tests/`), or any other front end without
   change. CMake also builds it as the static library `skinpeeler_core`.
3. **UI layer** (`src/views/`, `cli/skinpeeler_cli.cpp`) — the Qt 6 desktop
   GUI and the batch command-line program. Both only pass user input to
   engine calls and render or write engine output; neither performs any
   algorithmic computation itself.

## Key classes

| Layer | Class | Role |
|---|---|---|
| Engine (model) | `CTImage` | Owns the state of one CT slice: DICOM metadata and pixel data (via DCMTK), quasi-triangulation/beta-complex construction (via `ImageProcessingBC`), shell extraction, cutting-point detection, bed-removal orchestration, and skin peeling / re-serialization for export. `calculateShells()` evaluates the slice at its beta; `sweepBetas()` evaluates a list of beta values on one Voronoi structure. `shellMask()` / `skinMask()` / `noAirMasks()` rasterize shells to pixel masks. |
| Engine (model) | `CTShell` | A single extracted shell: its list of quasi-triangulation vertices (disk position + radius) and the shell's (mu, sigma) intensity statistics. |
| Engine (model) | `BedDetector` | Detects CT-bed components among beta-complex components in a slice, using position/circularity/width/curvature/convexity checks. Its scoring helpers (`fitPolynomial`, `maxCurvature`, `checkSagAndChord`) are public static pure functions, designed for unit-testability without a `CTImage` or DICOM file. |
| Engine (model) | `BedCache` | Disk-backed cache of per-slice `BedDetector::Result`, keyed by specimen directory, so bed detection does not need to be recomputed across sessions. |
| Engine (model) | `ShellStatistics` | Stateless free functions operating on plain `(mu, sigma)` values: 2x2 covariance inversion, Mahalanobis distance, the BIC changepoint search, the Mahalanobis + BIC cutting-point computation (the default cut rule), and the 3-sigma-ellipse and Euclidean-jump alternatives. This is the mathematical core of the cut decision, deliberately decoupled from `CTImage`, V-ImageProcessing, and DCMTK so it can be unit-tested with synthetic inputs. |
| Engine (model) | `BetaSelection` | Stateless functions over plain per-slice records: twelve label-free rules that select one beta for a specimen from results computed at several beta values, each returning the selected beta and a per-beta score table. No dependency on `CTImage`, V-ImageProcessing, or DCMTK. |
| Engine (service) | `DicomService` | Finds and sorts `.dcm` files in a directory, batch-loads them into `CTImage` objects, normalizes intensity across slices to a common maximum, and batch-saves peeled output. |
| Engine (service) | `PeelingService` | Batch peeling across a loaded volume: auto mode (each slice computes its own cut) and uniform mode (user-set cuts at key sections, linearly interpolated between them). Takes progress/cancel callbacks instead of touching Qt directly. |
| Engine (service) | `StatisticsService` | Collects per-slice `CTImage::Stats` into a `StatsList`, formats text reports, and computes inter-shell Euclidean distance summaries. |
| Engine (session) | `skinpeeler::Session` | The session flow in plain C++: load a directory, normalize, calculate slices, query shell statistics / fit / masks / beta-selection records, peel, save. `scan_series()` + `load_series_slice()` give the same slice order and normalization with one slice in memory at a time. |
| UI (command line) | `skinpeeler_cli` | Batch program with the commands `shells`, `select-beta`, and `peel`: argument parsing, progress lines, and CSV / PNG / JSON / DICOM writing over `Session`, `CTImage`, and `BetaSelection`. |
| UI (view) | `MainWindow` | The Qt main window: wires UI signals (open/save, navigate, adjust beta, peel, annotate) to engine calls, owns the background-task runner (`runEngineTask`, see below) and the progress bar / cancel button for batch peeling. |
| UI (view) | `SkinPeelerScene` | `QGraphicsScene`-based rendering of the current CT slice and its shell overlay, plus manual annotation painting. |
| UI (view) | `PlotWidgets` / `PlotWindow` | `QPainter`-based statistical plot panels (Gaussian intensity fit, shell `(mu, sigma)` scatter with the Mahalanobis threshold ellipse of the active cut rule, inter-shell distance histogram) shown in a separate plot window. |

## Shell traversal and the V-ImageProcessing build

`CTImage` extracts shells with one of two traversal implementations that
produce the same shells in the same vertex order
(`SKINPEELER_HAVE_SHELL_FRONT` in `src/models/CTImage.cpp`):

- the **shell-front functions** of `ImageProcessingBC`
  (`collect_outer_shell_front`, `expand_shell_front`,
  `select_componentBSFace_by_id`), which walk the raw Voronoi vertices and
  allocate nothing per shell;
- the **wrapper traversal** over `QuasiVertex` / `QuasiEdge` objects, used
  when the linked V-ImageProcessing library does not export the shell-front
  functions.

CMake detects which one applies by searching the linked library for the
function name (`-DSKINPEELER_SHELL_FRONT=ON|OFF` overrides the detection).
`skinpeeler_cli` is built only with the shell-front functions: with the
wrapper traversal the process memory grows with every evaluated beta and
slice, which a batch run over a whole series cannot afford. The bundled
Linux library provides the shell-front functions; the bundled Windows
library does not (see `third_party/v_imageprocessing/NOTICE.md`).

## Concurrency model

`CTImage` and the underlying V-ImageProcessing Voronoi/beta-complex engine
are **not thread-safe** — the library may hold process-wide state shared
across `CTImage` instances. `MainWindow::runEngineTask()` is the single
choke point that enforces this for the whole window:

- At most one engine-touching background task (shell calculation, peeling,
  DICOM load + first-slice calculation) may be in flight at a time, guarded
  by an `std::atomic<bool> m_calcRunning`.
- `runEngineTask(work, onDone)` runs `work` on a detached `std::thread`,
  then marshals `onDone(success, errorMessage)` back to the GUI thread via
  `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- While a task is running, the UI is disabled (`setUIEnabled(false)`) to
  block most entry points; the keyboard-shortcut slice-navigation path is
  additionally guarded explicitly, since it bypasses the disabled central
  widget.
- A slice-navigation request that arrives while a task is running is not
  dropped: it is coalesced into a single pending index (`m_pendingNavIndex`)
  and replayed once the in-flight task completes.
- Batch peeling (`PeelingService::peelAutoMode` / `peelUniformMode`)
  supports cooperative cancellation: a cancel callback is polled between
  slices and takes effect at the next slice boundary (already-peeled slices
  are not rolled back).

This single-flight design keeps the thread-safety argument simple: callers
never need to reason about which `CTImage` a worker thread currently holds,
only whether `m_calcRunning` is true.
