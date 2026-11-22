#pragma once

#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// BetaSelection
//
// Label-free, per-specimen selection of beta from per-slice results computed at
// several betas. Pure value types only (no CTImage, no geometry library, no
// DICOM), like ShellStatistics.
//
// Interface: rule id -> selected beta + per-beta score table. Callers pass
// per-slice records and a Query (rule, valid slices, candidate betas,
// thresholds); every rule goes through select().
//
// Rules (thresholds are Params fields with the pre-registered defaults;
// a tie always goes to the smaller beta):
//   A1  plateau entry: smallest candidate from which every adjacent pair of
//       candidates has mean agreement D >= a1Threshold (0.98)
//   A2  window agreement: largest mean of D(beta, beta') over the other
//       candidates within +-a2Window (0.02)
//   A3  cut stability: largest share of judgeable slices whose cut is the
//       same at every candidate within +-a3Window (0.01)
//   A4  plateau entry with a4Threshold (0.9741)
//   B1  BIC decisiveness: largest median of BIC0 - min BIC1
//   B2  separation contrast: largest median of log(d2[cut] / d2[cut+1])
//   B3  body homogeneity: smallest median of log det cov(mu, sigma) of the
//       body shells
//   B4  judgement failure: smallest share of valid slices without a BIC
//       changepoint
//   C1  onset + margin: per slice, the first beta (among all betas present)
//       whose area >= c1AreaFraction (0.9) x area at the reference beta; the
//       c1Percentile-th (90) percentile of those, rounded up to a candidate
//   C2  area plateau: smallest candidate from which every adjacent pair has
//       median |area(next)/area - 1| <= c2Epsilon (0.01)
//   C3a Betti curve: first beta of the longest run of candidates with the
//       same (median n_components, median n_inner_loops)
//   C4  shell-count plateau: smallest candidate from which the median number
//       of shells changes by at most c4MaxChange (1) between adjacent pairs
//
// Agreement D(beta, beta') = mean over slices judgeable at both betas of the
// Dice of the skin shells S0..S_cut counted in vertices: 1 when the cuts are
// equal, otherwise min(1, 2 * sum_{i <= min cut} min(n_i, n'_i) / (T + T')),
// T = number of skin-shell vertices; slices with T + T' == 0 are skipped.
// =============================================================================
namespace BetaSelection {

// -------------------------------------------------------------------------
// b2SeparationContrast — per slice and beta:
//     log( max(d2[cut], 1e-12) / d2[cut + 1] )
// (last skin shell against first body shell), where d2 are the squared
// Mahalanobis distances of the cutting rule's fit and `cut` its cut index.
//
// `mahalDByShell[s]` = Mahalanobis distance of shell index s, negative when
// the shell was not scored. `firstFitShell` = index of the first shell that
// enters the fit (S2: 2 when the outermost shell is excluded).
//
// Defined only when cut >= 2 and both shells cut and cut+1 are fit shells
// (scored, index >= firstFitShell) and d2[cut + 1] > 0; otherwise NaN.
// -------------------------------------------------------------------------
double b2SeparationContrast(const std::vector<double>& mahalDByShell, int cut, int firstFitShell);

// One slice at one beta.
struct SliceRecord {
    int sliceIndex = 0;
    double beta = 0.0;
    // false: the evaluation failed (no result). The record still counts as a
    // valid slice that is not judgeable; it does not enter component / shell
    // count statistics.
    bool evaluated = true;
    bool judgeable = false;  // the cutting rule could be evaluated (enough shells for the fit)
    bool changepointFound = false;
    int cut = 0;  // meaningful when judgeable
    double b2Contrast = std::numeric_limits<double>::quiet_NaN();

    // Component / shell-count quantities (C rules)
    int nShells = 0;
    int nComponents = 0;
    double area = std::numeric_limits<double>::quiet_NaN();  // selected component
    int nInnerLoops = -1;  // selected component; -1 = not available

    // Per-shell quantities (A rules, B1, B3); filled by fillFromShells().
    bool hasShellData = false;
    double dBic = std::numeric_limits<double>::quiet_NaN();        // BIC0 - min BIC1
    double bodyLogDet = std::numeric_limits<double>::quiet_NaN();  // log det cov of body shells
    std::vector<int> skinVertexCounts;                             // shells 0..cut
};

// One shell of a slice (index 0 = outermost).
struct ShellRow {
    int shellIndex = 0;
    double mu = std::numeric_limits<double>::quiet_NaN();
    double sigma = std::numeric_limits<double>::quiet_NaN();
    int nVertices = 0;
    double d2 =
        std::numeric_limits<double>::quiet_NaN();  // squared Mahalanobis distance; NaN = not scored
};

// Fills the per-shell quantities of `rec` (which must already carry
// judgeable / cut) from the slice's shells, sorted by shell index from 0:
//   skinVertexCounts = vertex counts of shells 0..cut
//   dBic       = ShellStatistics::bicChangepointTerms(d2 of the fit shells S2..Sm).deltaBic()
//   bodyLogDet = log det of the sample covariance (N-1) of (mu, sigma) over
//                shells cut+1..m when there are >= 3 of them and det > 0
// For a non-judgeable record only hasShellData is set. `firstFitShell` as in
// b2SeparationContrast().
void fillFromShells(SliceRecord& rec, const std::vector<ShellRow>& shells, int firstFitShell);

enum class Rule { A1, A2, A3, A4, B1, B2, B3, B4, C1, C2, C3a, C4 };
const std::vector<Rule>& allRules();
const char* ruleName(Rule rule);
bool parseRule(const std::string& text, Rule& out);
// What BetaScore::score holds for the rule.
const char* scoreName(Rule rule);

// Thresholds (pre-registered defaults).
struct Params {
    double a1Threshold = 0.98;
    double a4Threshold = 0.9741;
    double a2Window = 0.02;
    double a3Window = 0.01;
    double c1AreaFraction = 0.9;
    double c1Percentile = 90.0;
    // Reference beta of C1; NaN = the largest beta present in the records.
    double c1ReferenceBeta = std::numeric_limits<double>::quiet_NaN();
    double c2Epsilon = 0.01;
    double c4MaxChange = 1.0;
};
// Sets a parameter by name ("a1_threshold", "a4_threshold", "a2_window",
// "a3_window", "c1_area_fraction", "c1_percentile", "c1_reference_beta",
// "c2_epsilon", "c4_max_change"). Returns false for an unknown name.
bool setParam(Params& params, const std::string& name, double value);
std::vector<std::pair<std::string, double>> listParams(const Params& params);

struct Query {
    Rule rule = Rule::B2;
    // Valid slices (e.g. tail excluded). restrictSlices == false = all.
    bool restrictSlices = false;
    std::set<int> validSlices;
    // Candidate betas: betaMin <= beta <= betaMax (inclusive, 1e-9 tolerance).
    bool restrictBetas = false;
    double betaMin = 0.0, betaMax = 1.0;
    Params params;
};

// Per-beta row of the score table (candidate betas only, ascending beta).
struct BetaScore {
    double beta = 0.0;
    int nValid = 0;  // valid slice records at this beta
    int nJudgeable = 0;
    double judgeableShare = std::numeric_limits<double>::quiet_NaN();  // nJudgeable / nValid
    int nCutGe2 = 0;  // judgeable slices with cut >= 2
    double cutGe2Share = std::numeric_limits<double>::quiet_NaN();  // nCutGe2 / nJudgeable
    int nUsed = 0;  // slices (or slice pairs) entering the score
    double score = std::numeric_limits<double>::quiet_NaN();  // see scoreName(rule)
};

struct Selection {
    Rule rule = Rule::B2;
    bool selected = false;  // false: not decidable (see reason)
    double beta = std::numeric_limits<double>::quiet_NaN();
    double scoreAtSelected = std::numeric_limits<double>::quiet_NaN();
    std::string reason;  // why not decidable (empty when selected)
    std::vector<BetaScore> table;
    // Rule-level scalars (e.g. C1: the percentile value and reference beta).
    std::vector<std::pair<std::string, double>> diagnostics;
};

// Betas are grouped by exact value.
Selection select(const std::vector<SliceRecord>& records, const Query& query);

// Median with the usual even-count convention (mean of the two middle values);
// NaN for an empty input. NaN elements must be removed by the caller.
double median(std::vector<double> values);
// Percentile (0..100) with linear interpolation between order statistics;
// NaN for an empty input.
double percentile(std::vector<double> values, double q);

}  // namespace BetaSelection
