cmake_minimum_required(VERSION 3.16)
project(SKINPEELER VERSION 2.0 LANGUAGES CXX)

# =============================================================================
# SKINPEELER build notes
# =============================================================================
#
# Dependencies (Ubuntu / Debian):
#   sudo apt install qt6-base-dev libdcmtk-dev
#   (Qt 5 >= 5.13 is also supported as a fallback — see below.)
#
# V-ImageProcessing (third-party Voronoi/beta-complex library):
#   By default this project links the prebuilt static library and headers
#   bundled under third_party/v_imageprocessing/ (Linux x86_64 only for now).
#   TODO: a Windows (MSVC) prebuilt .lib is not yet bundled — building on
#   Windows currently requires -DV_IP_SOURCE_ROOT pointing at an internal
#   source checkout (lab-internal use only; see below).
#
# Internal/lab build (private source checkout available):
#   cmake -B build -DV_IP_SOURCE_ROOT=/path/to/PyMGOS/dev_vdrc/src/V
#
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt auto-generation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/src/views")

# =============================================================================
# V-ImageProcessing — prebuilt (default) or source build (-DV_IP_SOURCE_ROOT=)
# =============================================================================
if(DEFINED V_IP_SOURCE_ROOT)
    # ---- Lab-internal source build -----------------------------------------
    # Not for public use: requires a private V-ImageProcessing (PyMGOS) source
    # checkout. Mirrors the source-tree layout of that checkout.
    message(STATUS "SKINPEELER: building V-ImageProcessing from source at ${V_IP_SOURCE_ROOT}")

    set(V_ROOT      "${V_IP_SOURCE_ROOT}")
    set(VUFS_ROOT   "${V_ROOT}/VUFS")
    set(V_GEO_ROOT  "${V_ROOT}/Geo_tier/VoronoiDiagram2D")
    set(V_CORE_ROOT "${V_ROOT}/Core_tier/2D")
    set(V_API_ROOT  "${V_ROOT}/API_tier/2D")

    # NOTE: CONFIGURE_DEPENDS re-globs on every build-system regeneration,
    # which is slower than an explicit source list but tolerable for an
    # internal/occasional-use path. Keep the explicit list below (used by the
    # public prebuilt path) as the source of truth for what V_2D needs.
    file(GLOB_RECURSE VUFS_SOURCES CONFIGURE_DEPENDS "${VUFS_ROOT}/src/*.cpp")
    list(FILTER VUFS_SOURCES EXCLUDE REGEX ".*/different_version/.*")
    list(FILTER VUFS_SOURCES EXCLUDE REGEX ".*/backup/.*")
    list(FILTER VUFS_SOURCES EXCLUDE REGEX ".*\\.bak$")

    file(GLOB V_GEO_QT_SOURCES  CONFIGURE_DEPENDS "${V_GEO_ROOT}/src/QT/*.cpp")
    file(GLOB V_GEO_VD_SOURCES  CONFIGURE_DEPENDS "${V_GEO_ROOT}/src/VD/*.cpp")
    file(GLOB V_GEO_DVD_SOURCES CONFIGURE_DEPENDS "${V_GEO_ROOT}/src/DVD/*.cpp")
    list(FILTER V_GEO_VD_SOURCES  EXCLUDE REGEX ".*\\.bak$")
    list(FILTER V_GEO_DVD_SOURCES EXCLUDE REGEX ".*\\.bak$")

    file(GLOB V_CORE_SOURCES CONFIGURE_DEPENDS "${V_CORE_ROOT}/src/*.cpp")

    # API tier 2D — only files needed by ImageProcessingBC.
    set(V_API_SOURCES
        "${V_API_ROOT}/src/ImageProcessingBC.cpp"
        "${V_API_ROOT}/src/AlphaCplx.cpp"
        "${V_API_ROOT}/src/BetaCplx.cpp"
        "${V_API_ROOT}/src/DelaunayTrgl.cpp"
        "${V_API_ROOT}/src/EdgeGenerator.cpp"
        "${V_API_ROOT}/src/FileIOFunction.cpp"
        "${V_API_ROOT}/src/PointGenerator.cpp"
        "${V_API_ROOT}/src/PointVoroDgrm.cpp"
        "${V_API_ROOT}/src/QuasiEdge.cpp"
        "${V_API_ROOT}/src/QuasiFace.cpp"
        "${V_API_ROOT}/src/QuasiTrgl.cpp"
        "${V_API_ROOT}/src/QuasiVertex.cpp"
        "${V_API_ROOT}/src/SimpleTypeForDVD.cpp"
        "${V_API_ROOT}/src/VertexGenerator.cpp"
        "${V_API_ROOT}/src/VoroConst.cpp"
        "${V_API_ROOT}/src/VoroEdge.cpp"
        "${V_API_ROOT}/src/VoroFace.cpp"
        "${V_API_ROOT}/src/VoroVertex.cpp"
    )

    add_library(V_2D STATIC
        ${VUFS_SOURCES}
        ${V_GEO_QT_SOURCES}
        ${V_GEO_VD_SOURCES}
        ${V_GEO_DVD_SOURCES}
        ${V_CORE_SOURCES}
        ${V_API_SOURCES}
    )
    target_include_directories(V_2D PUBLIC
        "${VUFS_ROOT}/include"
        "${V_GEO_ROOT}/include"
        "${V_CORE_ROOT}/include"
        "${V_API_ROOT}/include"
    )
    target_compile_features(V_2D PUBLIC cxx_std_17)
    target_compile_options(V_2D PRIVATE -fPIC)
    if(WIN32)
        target_compile_definitions(V_2D PRIVATE _USE_MATH_DEFINES NOMINMAX)
    endif()

    set(V_IMAGEPROCESSING_TARGET V_2D)
else()
    # ---- Public path: prebuilt binary + headers ----------------------------
    set(V_IP_ROOT "${CMAKE_SOURCE_DIR}/third_party/v_imageprocessing")

    add_library(V_2D STATIC IMPORTED GLOBAL)

    if(WIN32)
        set(V_IP_LIB "${V_IP_ROOT}/lib/win-x64/V_2D.lib")
    else()
        set(V_IP_LIB "${V_IP_ROOT}/lib/linux-x86_64/libV_2D.a")
    endif()

    if(NOT EXISTS "${V_IP_LIB}")
        message(FATAL_ERROR
            "SKINPEELER: prebuilt V-ImageProcessing library not found at "
            "${V_IP_LIB}. On Windows this is expected (not yet bundled — "
            "see third_party/v_imageprocessing/NOTICE.md). Either obtain a "
            "prebuilt library for your platform, or (lab-internal) pass "
            "-DV_IP_SOURCE_ROOT=<path-to-PyMGOS-V-checkout> to build from "
            "source.")
    endif()

    set_target_properties(V_2D PROPERTIES
        IMPORTED_LOCATION "${V_IP_LIB}"
    )
    target_include_directories(V_2D INTERFACE
        "${V_IP_ROOT}/include/VUFS/include"
        "${V_IP_ROOT}/include/Geo_tier/VoronoiDiagram2D/include"
        "${V_IP_ROOT}/include/API_tier/2D/include"
    )
    target_compile_features(V_2D INTERFACE cxx_std_17)

    set(V_IMAGEPROCESSING_TARGET V_2D)
endif()

# =============================================================================
# Qt6 (preferred) / Qt5 >= 5.13 fallback
# =============================================================================
find_package(Qt6 COMPONENTS Core Widgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Core Qt6::Widgets)
    message(STATUS "SKINPEELER: using Qt6")
else()
    find_package(Qt5 5.13 COMPONENTS Core Widgets REQUIRED)
    set(QT_LIBS Qt5::Core Qt5::Widgets)
    message(STATUS "SKINPEELER: using Qt5 (>= 5.13)")
endif()

# =============================================================================
# DCMTK
# =============================================================================
find_package(DCMTK REQUIRED)

# JPEG Lossless codec libraries (Transfer Syntax 1.2.840.10008.1.2.4.70),
# required by DJDecoderRegistration::registerCodecs() in CTImage.cpp.
# Ship with libdcmtk-dev on Debian/Ubuntu.
find_library(DCMJPEG_LIB dcmjpeg REQUIRED)
find_library(IJG8_LIB    ijg8    REQUIRED)
find_library(IJG12_LIB   ijg12   REQUIRED)
find_library(IJG16_LIB   ijg16   REQUIRED)
set(DCMJPEG_LIBS ${DCMJPEG_LIB} ${IJG8_LIB} ${IJG12_LIB} ${IJG16_LIB})

# =============================================================================
# SKINPEELER engine + GUI sources (explicit list — no GLOB)
# =============================================================================
set(SKINPEELER_ENGINE_SOURCES
    src/models/CTShell.cpp
    src/models/ShellStatistics.cpp
    src/models/CTImage.cpp
    src/models/BedDetector.cpp
    src/models/BedCache.cpp
    src/services/DicomService.cpp
    src/services/PeelingService.cpp
    src/services/StatisticsService.cpp
)

set(SKINPEELER_GUI_SOURCES
    src/main.cpp
    src/views/MainWindow.cpp
    src/views/SkinPeelerScene.cpp
    src/views/PlotWidgets.cpp
    src/views/PlotWindow.cpp
)

set(SKINPEELER_HEADERS
    src/config/Settings.h
    src/models/CTShell.h
    src/models/ShellStatistics.h
    src/models/CTImage.h
    src/models/BedDetector.h
    src/models/BedCache.h
    src/services/DicomService.h
    src/services/PeelingService.h
    src/services/StatisticsService.h
    src/utils/CoordinateTransform.h
    src/views/MainWindow.h
    src/views/SkinPeelerScene.h
    src/views/PlotWidgets.h
    src/views/PlotWindow.h
)

add_executable(SKINPEELER
    ${SKINPEELER_ENGINE_SOURCES}
    ${SKINPEELER_GUI_SOURCES}
    ${SKINPEELER_HEADERS}
    src/views/MainWindow.ui
    assets/resources.qrc
)

target_include_directories(SKINPEELER PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${DCMTK_INCLUDE_DIRS}"
)

target_link_libraries(SKINPEELER PRIVATE
    ${V_IMAGEPROCESSING_TARGET}
    ${QT_LIBS}
    ${DCMTK_LIBRARIES}
    ${DCMJPEG_LIBS}
)

if(WIN32)
    target_compile_definitions(SKINPEELER PRIVATE _USE_MATH_DEFINES NOMINMAX)
endif()

# =============================================================================
# Experiment CLI targets (no Qt dependency — DCMTK + V_2D only)
# =============================================================================
function(add_skinpeeler_cli name)
    add_executable(${name}
        cli/${name}.cpp
        ${SKINPEELER_ENGINE_SOURCES}
    )
    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/cli"
        "${DCMTK_INCLUDE_DIRS}"
    )
    target_link_libraries(${name} PRIVATE
        ${V_IMAGEPROCESSING_TARGET}
        ${DCMTK_LIBRARIES}
        ${DCMJPEG_LIBS}
    )
    if(WIN32)
        target_compile_definitions(${name} PRIVATE _USE_MATH_DEFINES NOMINMAX)
    endif()
endfunction()

add_skinpeeler_cli(exp_beta_sweep)
add_skinpeeler_cli(exp_face_rho)
add_skinpeeler_cli(exp_shell_export)
add_skinpeeler_cli(exp_shell_pixel_export)
add_skinpeeler_cli(exp_component_jaccard)
add_skinpeeler_cli(exp_skin_mask_dump)
add_skinpeeler_cli(check_shell0)

# =============================================================================
# Install
# =============================================================================
install(TARGETS SKINPEELER DESTINATION bin)

# TODO (Windows packaging): run windeployqt on the built SKINPEELER.exe to
# bundle the required Qt DLLs, e.g.:
#   add_custom_command(TARGET SKINPEELER POST_BUILD
#       COMMAND "${QT_DEPLOY_TOOL}" $<TARGET_FILE:SKINPEELER>)
# windeployqt itself is not invoked here since it requires a Windows/Qt
# environment not assumed to be present on every build machine.
