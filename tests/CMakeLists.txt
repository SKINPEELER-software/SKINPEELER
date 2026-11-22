# =============================================================================
# SKINPEELER unit tests — GoogleTest via FetchContent + CTest
# =============================================================================
#
# One test binary (skinpeeler_tests) links the full engine (same sources as
# the SKINPEELER/CLI targets) so that stage-3 integration tests can exercise
# CTImage::calculateShells() end-to-end against the bundled phantom DICOM
# sample data, alongside the pure-function unit tests.
# =============================================================================

include(FetchContent)

# Pinned version — do not float to main.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Engine sources use relative paths (resolved against CMAKE_SOURCE_DIR, the
# top-level directory where SKINPEELER_ENGINE_SOURCES was defined) — make
# them absolute so add_executable() resolves correctly from tests/.
set(SKINPEELER_ENGINE_SOURCES_ABS "")
foreach(_src ${SKINPEELER_ENGINE_SOURCES})
    list(APPEND SKINPEELER_ENGINE_SOURCES_ABS "${CMAKE_SOURCE_DIR}/${_src}")
endforeach()

add_executable(skinpeeler_tests
    test_shell_statistics.cpp
    test_ctshell.cpp
    test_bed_detector.cpp
    test_peeling_service.cpp
    test_statistics_service.cpp
    test_engine_integration.cpp
    test_beta_selection.cpp
    test_engine_sweep.cpp
    ${SKINPEELER_ENGINE_SOURCES_ABS}
)

target_include_directories(skinpeeler_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${DCMTK_INCLUDE_DIRS}"
)

target_link_libraries(skinpeeler_tests PRIVATE
    ${V_IMAGEPROCESSING_TARGET}
    ${DCMTK_LINK_LIBS}
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(skinpeeler_tests PRIVATE _USE_MATH_DEFINES NOMINMAX)
endif()

# Full path to the bundled phantom DICOM sample data (stage-3 integration test).
target_compile_definitions(skinpeeler_tests PRIVATE
    SKINPEELER_SAMPLE_DATA_DIR="${CMAKE_SOURCE_DIR}/sample_data"
)

include(GoogleTest)
gtest_discover_tests(skinpeeler_tests)

# =============================================================================
# skinpeeler_cli smoke tests on the bundled phantom series (only when the
# batch program is built -- see the shell-front section of the top-level
# CMakeLists.txt). `shells` writes per-slice results for two beta values of
# the first two slices; `select-beta` then reads that output directory.
# =============================================================================
if(TARGET skinpeeler_cli)
    set(_cli_smoke_dir "${CMAKE_CURRENT_BINARY_DIR}/cli_smoke")

    add_test(NAME skinpeeler_cli.version COMMAND skinpeeler_cli --version)

    add_test(NAME skinpeeler_cli.shells
        COMMAND skinpeeler_cli shells "${CMAKE_SOURCE_DIR}/sample_data/phantom"
                --out "${_cli_smoke_dir}/shells"
                --beta-range 0.65 0.70 0.05 --range 0:2 --masks --overwrite)
    set_tests_properties(skinpeeler_cli.shells PROPERTIES FIXTURES_SETUP cli_shells_output)

    add_test(NAME skinpeeler_cli.select_beta
        COMMAND skinpeeler_cli select-beta "${_cli_smoke_dir}/shells"
                --rule all --overwrite)
    set_tests_properties(skinpeeler_cli.select_beta PROPERTIES
        FIXTURES_REQUIRED cli_shells_output)

    add_test(NAME skinpeeler_cli.peel
        COMMAND skinpeeler_cli peel "${CMAKE_SOURCE_DIR}/sample_data/phantom"
                --out "${_cli_smoke_dir}/peel" --beta 0.69 --overwrite)

    add_test(NAME skinpeeler_cli.usage_error COMMAND skinpeeler_cli no-such-command)
    set_tests_properties(skinpeeler_cli.usage_error PROPERTIES WILL_FAIL TRUE)
endif()
