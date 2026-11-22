// =============================================================================
// Engine tests on the bundled phantom DICOM series: evaluating several beta
// values on one Voronoi structure (CTImage::sweepBetas), shell / skin /
// air-excluded masks, and the session layer (skinpeeler::Session).
// =============================================================================
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/Session.h"
#include "models/BetaSelection.h"
#include "models/CTImage.h"

#ifndef SKINPEELER_SAMPLE_DATA_DIR
#define SKINPEELER_SAMPLE_DATA_DIR "sample_data"
#endif

namespace {

std::string phantomDir() {
    return std::string(SKINPEELER_SAMPLE_DATA_DIR) + "/phantom";
}

CTImage loadPhantomSlice(double beta) {
    CTImage img = CTImage::fromDicomFile(phantomDir() + "/PHANTOM_CT_0001.dcm", beta, 10);
    img.normalizeToMax(65535.0);
    return img;
}

struct Snapshot {
    double beta = 0.0;
    int cut = 0;
    CTImage::CutRule cutRule = CTImage::CutRule::Unknown;
    CTImage::ComponentSummary component;
    std::vector<double> mu, sigma;
    std::vector<int> nVertices;
    std::vector<uint8_t> skin;
};

Snapshot snapshotOf(const CTImage& img) {
    Snapshot s;
    s.beta = img.beta();
    s.cut = img.algoPeelIndex();
    s.cutRule = img.cutRule();
    s.component = img.componentSummary();
    for (const auto& shell : img.shells()) {
        s.mu.push_back(shell.mu());
        s.sigma.push_back(shell.sigma());
        s.nVertices.push_back(static_cast<int>(shell.size()));
    }
    s.skin = img.skinMask();
    return s;
}

}  // namespace

TEST(EngineSweep, SweepBetasMatchesCalculateShellsAtEachBeta) {
    // Not monotone on purpose: every evaluation after the first re-traverses a
    // structure whose vertices were already visited.
    const std::vector<double> betas = {0.75, 0.60, 0.69};

    CTImage swept = loadPhantomSlice(0.5);
    std::vector<Snapshot> fromSweep;
    swept.sweepBetas(betas, [&](const CTImage::BetaEvaluation& ev) {
        ASSERT_TRUE(ev.ok) << ev.error;
        EXPECT_EQ(ev.shells.size(), swept.shells().size());
        EXPECT_EQ(ev.cut, swept.algoPeelIndex());
        fromSweep.push_back(snapshotOf(swept));
    });
    ASSERT_EQ(fromSweep.size(), betas.size());
    EXPECT_DOUBLE_EQ(swept.beta(), betas.back());

    for (size_t i = 0; i < betas.size(); ++i) {
        CTImage single = loadPhantomSlice(betas[i]);
        single.calculateShells();
        single.releaseVoronoi();
        const Snapshot expected = snapshotOf(single);

        SCOPED_TRACE("beta " + std::to_string(betas[i]));
        EXPECT_EQ(fromSweep[i].beta, expected.beta);
        EXPECT_EQ(fromSweep[i].cut, expected.cut);
        EXPECT_EQ(fromSweep[i].cutRule, expected.cutRule);
        EXPECT_EQ(fromSweep[i].component.nComponents, expected.component.nComponents);
        EXPECT_EQ(fromSweep[i].component.area, expected.component.area);
        EXPECT_EQ(fromSweep[i].component.nInnerLoops, expected.component.nInnerLoops);
        EXPECT_EQ(fromSweep[i].nVertices, expected.nVertices);
        EXPECT_EQ(fromSweep[i].mu, expected.mu);  // bit-identical statistics
        EXPECT_EQ(fromSweep[i].sigma, expected.sigma);
        EXPECT_EQ(fromSweep[i].skin, expected.skin);
    }
}

TEST(EngineSweep, EmptyBetaListIsNoOp) {
    CTImage img = loadPhantomSlice(0.69);
    int calls = 0;
    img.sweepBetas({}, [&](const CTImage::BetaEvaluation&) { ++calls; });
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(img.hasShells());
    EXPECT_DOUBLE_EQ(img.beta(), 0.69);
}

TEST(EngineSweep, CutRuleAndComponentSummaryFollowTheEvaluation) {
    CTImage img = loadPhantomSlice(0.69);
    EXPECT_EQ(img.cutRule(), CTImage::CutRule::Unknown);
    EXPECT_FALSE(img.componentSummary().valid);

    img.calculateShells();
    img.releaseVoronoi();
    EXPECT_NE(img.cutRule(), CTImage::CutRule::Unknown);
    EXPECT_STRNE(CTImage::cutRuleName(img.cutRule()), "UNKNOWN");
    ASSERT_TRUE(img.componentSummary().valid);
    EXPECT_GE(img.componentSummary().nComponents, 1);
    EXPECT_GT(img.componentSummary().area, 0.0);
    EXPECT_EQ(img.judgeable(), img.mahalFit().active);

    const CTImage::BetaEvaluation ev = img.summarizeEvaluation();
    EXPECT_EQ(ev.cut, img.algoPeelIndex());
    EXPECT_EQ(ev.shells.size(), img.shells().size());
    EXPECT_EQ(ev.judgeable, img.judgeable());

    img.setBeta(0.70);  // derived state is cleared
    EXPECT_EQ(img.cutRule(), CTImage::CutRule::Unknown);
    EXPECT_FALSE(img.componentSummary().valid);
}

TEST(EngineMasks, ShellMaskAndSkinMaskAreConsistent) {
    CTImage img = loadPhantomSlice(0.69);
    img.calculateShells();
    img.releaseVoronoi();
    const size_t nPixels = static_cast<size_t>(img.height()) * static_cast<size_t>(img.width());
    const int cut = img.peelIndex();

    const std::vector<uint8_t> skin = img.skinMask();
    ASSERT_EQ(skin.size(), nPixels);
    EXPECT_EQ(skin, img.shellMask(0, cut));
    EXPECT_EQ(skin, img.skinMask(cut));

    // Union of the per-shell masks is the range mask; out-of-range bounds clamp.
    std::vector<uint8_t> unionMask(nPixels, 0);
    for (int s = 0; s <= cut; ++s) {
        const std::vector<uint8_t> one = img.shellMask(s, s);
        for (size_t i = 0; i < nPixels; ++i)
            unionMask[i] |= one[i];
    }
    EXPECT_EQ(unionMask, skin);
    EXPECT_EQ(img.shellMask(-5, 1 << 20), img.shellMask(0, static_cast<int>(img.shells().size())));

    size_t nSet = 0;
    for (uint8_t v : skin) {
        ASSERT_LE(v, 1);
        nSet += v;
    }
    EXPECT_GT(nSet, 0u);
}

TEST(EngineMasks, NoAirMasksFollowTheDefinition) {
    CTImage img = loadPhantomSlice(0.69);
    img.calculateShells();
    img.releaseVoronoi();
    const size_t nPixels = static_cast<size_t>(img.height()) * static_cast<size_t>(img.width());

    const std::vector<uint8_t> s0 = img.shellMask(0, 0);
    const std::vector<uint8_t> cutMask = img.skinMask();
    const CTImage::NoAirMasks masks = img.noAirMasks();
    ASSERT_EQ(masks.skin.size(), nPixels);
    ASSERT_EQ(masks.air.size(), nPixels);
    ASSERT_FALSE(masks.s0Empty);

    double threshold = -1.0;
    int nS0 = 0;
    for (size_t i = 0; i < nPixels; ++i)
        if (s0[i]) {
            ++nS0;
            threshold = std::max(threshold, static_cast<double>(img.image()[i]));
        }
    EXPECT_EQ(masks.nS0Pixels, nS0);
    EXPECT_DOUBLE_EQ(masks.airThreshold, threshold);

    int nSkin = 0, nAir = 0;
    for (size_t i = 0; i < nPixels; ++i) {
        const bool above = static_cast<double>(img.image()[i]) > threshold;
        const uint8_t expectedSkin = (cutMask[i] && !s0[i] && above) ? 1 : 0;
        const uint8_t expectedAir = (s0[i] || !above) ? 1 : 0;
        ASSERT_EQ(masks.skin[i], expectedSkin) << "pixel " << i;
        ASSERT_EQ(masks.air[i], expectedAir) << "pixel " << i;
        ASSERT_FALSE(masks.skin[i] && masks.air[i]) << "pixel " << i;
        nSkin += expectedSkin;
        nAir += expectedAir;
    }
    EXPECT_EQ(masks.nSkinPixels, nSkin);
    EXPECT_EQ(masks.nAirPixels, nAir);
}

TEST(EngineSession, ScannedSliceEqualsSessionSlice) {
    using skinpeeler::Session;

    Session session;
    session.load_directory(phantomDir(), 0.69);
    ASSERT_EQ(session.slice_count(), 8);

    const Session::SeriesScan scan = Session::scan_series(phantomDir());
    ASSERT_EQ(scan.slice_count(), session.slice_count());
    EXPECT_GT(scan.global_max, 0.0);

    for (int idx : {0, 5}) {
        SCOPED_TRACE("slice " + std::to_string(idx));
        const std::unique_ptr<CTImage> single = Session::load_series_slice(scan, idx, 0.69);
        ASSERT_TRUE(single);
        const CTImage& fromSession = session.image(idx);
        ASSERT_EQ(single->width(), fromSession.width());
        ASSERT_EQ(single->height(), fromSession.height());
        EXPECT_EQ(single->image(), fromSession.image());  // same pixels after normalization
        EXPECT_EQ(std::filesystem::path(scan.slice(idx).path).filename().string(),
                  std::filesystem::path(session.slice_info(idx).filename).filename().string());
    }
}

TEST(EngineSession, CalculateSliceAndQueries) {
    using skinpeeler::Session;

    Session session;
    session.load_directory(phantomDir(), 0.69);

    EXPECT_THROW(session.segmentation_mask(0), std::runtime_error);
    EXPECT_THROW(session.image(99), std::out_of_range);

    const Session::CalculateResult result = session.calculate_slice(0);
    EXPECT_GT(result.shell_count, 0);
    EXPECT_FALSE(result.timed_out);

    const CTImage& img = session.image(0);
    EXPECT_EQ(result.shell_count, static_cast<int>(img.shells().size()));
    EXPECT_EQ(result.peel_index, img.peelIndex());

    const std::vector<Session::ShellStat> stats = session.shell_stats(0);
    ASSERT_EQ(stats.size(), img.shells().size());
    EXPECT_EQ(stats[1].mu, img.shells()[1].mu());
    EXPECT_EQ(stats[1].n_vertices, static_cast<int>(img.shells()[1].size()));

    EXPECT_EQ(session.segmentation_mask(0), img.skinMask());
    EXPECT_EQ(session.no_air_masks(0).skin, img.noAirMasks().skin);

    const BetaSelection::SliceRecord rec = session.beta_selection_record(0);
    EXPECT_EQ(rec.sliceIndex, 0);
    EXPECT_DOUBLE_EQ(rec.beta, 0.69);
    EXPECT_EQ(rec.cut, img.algoPeelIndex());
    EXPECT_EQ(rec.nShells, result.shell_count);
    EXPECT_EQ(rec.judgeable, img.judgeable());
    EXPECT_TRUE(rec.hasShellData);
    EXPECT_EQ(rec.nComponents, img.componentSummary().nComponents);

    session.set_beta_all(0.70);  // clears computed shells
    EXPECT_FALSE(session.slice_info(0).has_shells);
}
