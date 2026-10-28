// =============================================================================
// Stage 3 test — one engine-level integration test against bundled phantom
// DICOM sample data. Exercises the full pipeline (DicomService load -> Voronoi
// construction -> shell extraction) end to end; a few seconds is expected.
// =============================================================================
#include <gtest/gtest.h>

#include <string>

#include "models/CTImage.h"

#ifndef SKINPEELER_SAMPLE_DATA_DIR
#define SKINPEELER_SAMPLE_DATA_DIR "sample_data"
#endif

namespace {
std::string phantomPath(const std::string& filename) {
    return std::string(SKINPEELER_SAMPLE_DATA_DIR) + "/phantom/" + filename;
}
}  // namespace

TEST(EngineIntegration, LoadPhantomSliceAndComputeShells) {
    CTImage img = CTImage::fromDicomFile(phantomPath("PHANTOM_CT_0001.dcm"), 0.69, 10);
    img.normalizeToMax(65535.0);

    ASSERT_NO_THROW(img.calculateShells());

    ASSERT_GT(img.shells().size(), 0u);

    // shell[0] is the outermost air/boundary shell, excluded from cutting
    // statistics -- its vertices carry (near) zero Voronoi disk radius, so
    // mu (mean diameter) should be (near) zero.
    const auto& shell0 = img.shells()[0];
    ASSERT_TRUE(shell0.hasStatistics());
    EXPECT_NEAR(shell0.mu(), 0.0, 1e-3);

    img.releaseVoronoi();
}
