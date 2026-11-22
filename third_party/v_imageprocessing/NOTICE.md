# V-ImageProcessing — Third-Party Binary Notice

This directory bundles **prebuilt static libraries** (`lib/linux-x86_64/libV_2D.a`,
`lib/win-x64/V_2D.lib`) and the public C++ headers (`include/`) for
**V-ImageProcessing**, a computational
geometry library (Voronoi diagram / beta-complex construction) that is
**proprietary software owned and developed by the Voronoi Diagram Research
Center (VDRC), Hanyang University**.

## Status

- The bundled binary is a performance- and correctness-patched build of
  V-ImageProcessing.
- The library's source code is **not** included in this repository and
  remains private to VDRC. Only the public headers required to compile
  against the library, and a prebuilt binary artifact, are bundled here —
  with the research center's permission — so that SKINPEELER can be built
  and run end-to-end without access to the private source repository.
- This binary/header bundle is **not covered by the MIT license** that
  applies to the rest of this repository (see top-level `LICENSE`). All
  rights to V-ImageProcessing remain with the Voronoi Diagram Research
  Center, Hanyang University.
- Redistribution of this prebuilt binary and these headers alongside
  SKINPEELER has been authorized by the research center for the specific
  purpose of allowing SKINPEELER to be built and used. This authorization
  does not extend to the library's source code, to independent
  redistribution of this bundle outside of the SKINPEELER project, or to
  any other use of V-ImageProcessing on its own.
- For access to the V-ImageProcessing source, licensing beyond the scope
  above, or any other questions about this library, please contact the
  Voronoi Diagram Research Center, Hanyang University directly.

## Platform coverage

| Library | Toolchain | Shell-front functions |
|---|---|---|
| `lib/linux-x86_64/libV_2D.a` | GCC 11, Release (`-O3 -DNDEBUG`), position-independent code, C++17 | yes |
| `lib/win-x64/V_2D.lib` | MSVC x64, Release | yes |

The headers in `include/` describe both libraries. The *shell-front
functions* are `ImageProcessingBC::select_componentBSFace_by_id`,
`collect_outer_shell_front`, and `expand_shell_front` (with the types
`ShellVertex` and `ShellFront`). The top-level `CMakeLists.txt` checks for
them by searching the linked library for the function name. When a library
without these functions is linked (for example an older build):

- the engine uses its `QuasiVertex` / `QuasiEdge` wrapper traversal, which
  produces the same shells;
- the GUI (`SKINPEELER`), the experiment tools, the engine library
  (`skinpeeler_core`), and the GoogleTest suite build and run;
- the batch command-line program `skinpeeler_cli` and its CTest smoke tests
  are **not built**.

## Rebuilding the bundle (lab-internal)

Rebuilding requires the private V-ImageProcessing source checkout. Both
libraries are the `V_2D` target of this repository's own `CMakeLists.txt`,
configured with `-DV_IP_SOURCE_ROOT`.

**Linux** — one script builds the library, copies it into
`lib/linux-x86_64/`, and refreshes the bundled headers that differ from the
source checkout:

```bash
tools/rebuild_v_imageprocessing_bundle.sh /path/to/V-ImageProcessing/src/V
```

**Windows** (x64 Native Tools Command Prompt for Visual Studio, with the same
Qt and DCMTK setup used for a normal build):

```bat
cmake -B build_v_imageprocessing -DV_IP_SOURCE_ROOT=C:\path\to\V-ImageProcessing\src\V -DBUILD_TESTING=OFF
cmake --build build_v_imageprocessing --config Release --target V_2D
copy /Y build_v_imageprocessing\Release\V_2D.lib third_party\v_imageprocessing\lib\win-x64\V_2D.lib
```

A fresh configure of the normal build (`cmake -B build`) then reports
`V-ImageProcessing shell-front primitives: available` and builds
`skinpeeler_cli`.

## Building against this library

See the top-level `CMakeLists.txt`: by default it links the prebuilt static
library in this directory. An internal/lab build can instead point
`-DV_IP_SOURCE_ROOT=<path-to-source-checkout>` at a private source checkout to
build V-ImageProcessing from source.
