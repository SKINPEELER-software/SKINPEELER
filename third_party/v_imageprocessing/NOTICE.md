# V-ImageProcessing — Third-Party Binary Notice

This directory bundles a **prebuilt static library** (`lib/linux-x86_64/libV_2D.a`)
and its public C++ headers (`include/`) for **V-ImageProcessing**, a computational
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

- `lib/linux-x86_64/libV_2D.a` — built with GCC, Release (`-O3 -DNDEBUG`),
  position-independent code, C++17.
- Windows (`V_2D.lib`, MSVC) is **not yet included** (TODO — see top-level
  README build notes).

## Building against this library

See the top-level `CMakeLists.txt`: by default it links the prebuilt static
library in this directory. An internal/lab build can instead point
`-DV_IP_SOURCE_ROOT=<path-to-PyMGOS-checkout>` at a private source checkout to
build V-ImageProcessing from source.
