#include "BetaSelection.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include "ShellStatistics.h"

namespace BetaSelection {

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();
// Slack added to the beta windows of A2 / A3 so that grid neighbours at
// exactly the window distance are inside despite rounding (0.02 -> 0.0201).
constexpr double kWindowSlack = 1e-4;
}  // namespace

// =============================================================================
// Per-slice quantities
// =============================================================================
double b2SeparationContrast(const std::vector<double>& mahalDByShell, int cut, int firstFitShell) {
    const int n = static_cast<int>(mahalDByShell.size());

    if (cut < 2 || cut < firstFitShell)
        return kNaN;  // shell `cut` must be a fit shell
    if (cut + 1 >= n)
        return kNaN;  // first body shell must exist
    const double dCut = mahalDByShell[cut];
    const double dNext = mahalDByShell[cut + 1];
    if (dCut < 0.0 || dNext < 0.0)
        return kNaN;  // not scored

    const double d2Cut = dCut * dCut;
    const double d2Next = dNext * dNext;
    if (!(d2Next > 0.0))
        return kNaN;
    return std::log(std::max(d2Cut, 1e-12) / d2Next);
}

namespace {
// Determinant of the symmetric 2x2 matrix [[a, b], [b, d]] by LU with partial
// pivoting (the operation order of a LAPACK-based determinant).
double det2x2Symmetric(double a, double b, double d) {
    if (std::abs(a) >= std::abs(b)) {
        if (a == 0.0)
            return 0.0;
        const double l = b / a;
        return a * (d - l * b);
    }
    const double l = a / b;
    return -(b * (b - l * d));
}
}  // namespace

void fillFromShells(SliceRecord& rec, const std::vector<ShellRow>& shells, int firstFitShell) {
    rec.hasShellData = true;
    rec.dBic = kNaN;
    rec.bodyLogDet = kNaN;
    rec.skinVertexCounts.clear();
    if (!rec.evaluated || !rec.judgeable)
        return;

    const int n = static_cast<int>(shells.size());

    for (int i = 0; i < n && i <= rec.cut; ++i)
        rec.skinVertexCounts.push_back(shells[i].nVertices);

    // BIC decisiveness on the d2 sequence of the fit shells (S2..Sm)
    std::vector<double> d2;
    for (int i = std::max(0, firstFitShell); i < n; ++i)
        if (!std::isnan(shells[i].d2))
            d2.push_back(shells[i].d2);
    rec.dBic = ShellStatistics::bicChangepointTerms(d2).deltaBic();

    // Body homogeneity: log det of the sample covariance of (mu, sigma)
    std::vector<std::pair<double, double>> body;
    for (int i = rec.cut + 1; i < n; ++i)
        if (i >= 0 && !std::isnan(shells[i].mu) && !std::isnan(shells[i].sigma))
            body.emplace_back(shells[i].mu, shells[i].sigma);
    if (body.size() >= 3) {
        const double nb = static_cast<double>(body.size());
        double mMu = 0.0, mSg = 0.0;
        for (const auto& p : body) {
            mMu += p.first;
            mSg += p.second;
        }
        mMu /= nb;
        mSg /= nb;
        double c00 = 0.0, c01 = 0.0, c11 = 0.0;
        for (const auto& p : body) {
            const double dm = p.first - mMu, ds = p.second - mSg;
            c00 += dm * dm;
            c01 += dm * ds;
            c11 += ds * ds;
        }
        const double div = nb - 1.0;
        const double det = det2x2Symmetric(c00 / div, c01 / div, c11 / div);
        if (det > 0.0)
            rec.bodyLogDet = std::log(det);
    }
}

// =============================================================================
// Rule names / parameters
// =============================================================================
const std::vector<Rule>& allRules() {
    static const std::vector<Rule> rules = {Rule::A1, Rule::A2, Rule::A3,  Rule::A4,
                                            Rule::B1, Rule::B2, Rule::B3,  Rule::B4,
                                            Rule::C1, Rule::C2, Rule::C3a, Rule::C4};
    return rules;
}

const char* ruleName(Rule rule) {
    switch (rule) {
        case Rule::A1:
            return "A1";
        case Rule::A2:
            return "A2";
        case Rule::A3:
            return "A3";
        case Rule::A4:
            return "A4";
        case Rule::B1:
            return "B1";
        case Rule::B2:
            return "B2";
        case Rule::B3:
            return "B3";
        case Rule::B4:
            return "B4";
        case Rule::C1:
            return "C1";
        case Rule::C2:
            return "C2";
        case Rule::C3a:
            return "C3a";
        case Rule::C4:
            return "C4";
    }
    return "UNKNOWN";
}

bool parseRule(const std::string& text, Rule& out) {
    std::string t;
    for (char c : text)
        if (c != '-' && c != '_')
            t += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (t == "C3A") {
        out = Rule::C3a;
        return true;
    }
    for (Rule r : allRules()) {
        if (r == Rule::C3a)
            continue;
        if (t == ruleName(r)) {
            out = r;
            return true;
        }
    }
    return false;
}

const char* scoreName(Rule rule) {
    switch (rule) {
        case Rule::A1:
        case Rule::A4:
            return "mean_agreement_with_next_beta";
        case Rule::A2:
            return "mean_window_agreement";
        case Rule::A3:
            return "cut_stable_share";
        case Rule::B1:
            return "median_delta_bic";
        case Rule::B2:
            return "median_b2_contrast";
        case Rule::B3:
            return "median_body_log_det";
        case Rule::B4:
            return "no_changepoint_share";
        case Rule::C1:
            return "share_of_slices_merged";
        case Rule::C2:
            return "median_rel_area_change_to_next_beta";
        case Rule::C3a:
            return "median_n_components";
        case Rule::C4:
            return "median_n_shells";
    }
    return "score";
}

bool setParam(Params& p, const std::string& name, double value) {
    if (name == "a1_threshold")
        p.a1Threshold = value;
    else if (name == "a4_threshold")
        p.a4Threshold = value;
    else if (name == "a2_window")
        p.a2Window = value;
    else if (name == "a3_window")
        p.a3Window = value;
    else if (name == "c1_area_fraction")
        p.c1AreaFraction = value;
    else if (name == "c1_percentile")
        p.c1Percentile = value;
    else if (name == "c1_reference_beta")
        p.c1ReferenceBeta = value;
    else if (name == "c2_epsilon")
        p.c2Epsilon = value;
    else if (name == "c4_max_change")
        p.c4MaxChange = value;
    else
        return false;
    return true;
}

std::vector<std::pair<std::string, double>> listParams(const Params& p) {
    return {{"a1_threshold", p.a1Threshold},
            {"a4_threshold", p.a4Threshold},
            {"a2_window", p.a2Window},
            {"a3_window", p.a3Window},
            {"c1_area_fraction", p.c1AreaFraction},
            {"c1_percentile", p.c1Percentile},
            {"c1_reference_beta", p.c1ReferenceBeta},
            {"c2_epsilon", p.c2Epsilon},
            {"c4_max_change", p.c4MaxChange}};
}

// =============================================================================
// Order statistics
// =============================================================================
double median(std::vector<double> values) {
    if (values.empty())
        return kNaN;
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    if (n % 2 == 1)
        return values[n / 2];
    return 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

double percentile(std::vector<double> values, double q) {
    if (values.empty())
        return kNaN;
    std::sort(values.begin(), values.end());
    const double pos = static_cast<double>(values.size() - 1) * (q / 100.0);
    const double fl = std::floor(pos);
    const size_t lo = static_cast<size_t>(std::max(0.0, fl));
    const size_t hi = std::min(lo + 1, values.size() - 1);
    const double t = pos - fl;
    const double a = values[lo], b = values[hi];
    const double diff = b - a;
    // Linear interpolation, evaluated from the nearer end.
    return (t >= 0.5) ? b - diff * (1.0 - t) : a + diff * t;
}

// =============================================================================
// select
// =============================================================================
namespace {

using SliceMap = std::map<int, const SliceRecord*>;  // slice index -> record

struct Context {
    const Query& query;
    std::map<double, SliceMap> byBeta;  // every beta present (valid slices only)
    std::vector<double> grid;           // all betas present, ascending
    std::vector<double> cands;          // candidate betas, ascending
    bool shellDataComplete = true;      // every judgeable valid record has per-shell data
    bool innerLoopsComplete = true;     // every evaluated valid record has nInnerLoops

    const SliceMap& at(double beta) const {
        return byBeta.find(beta)->second;
    }
};

double meanOf(const std::vector<double>& v) {
    if (v.empty())
        return kNaN;
    double s = 0.0;
    for (double x : v)
        s += x;
    return s / static_cast<double>(v.size());
}

// Mean agreement D between two betas (see header). nPairs = slices entering.
double agreement(const Context& ctx, double betaA, double betaB, int* nPairs = nullptr) {
    const SliceMap& A = ctx.at(betaA);
    const SliceMap& B = ctx.at(betaB);
    std::vector<double> dice;
    for (const auto& kv : A) {
        const auto it = B.find(kv.first);
        if (it == B.end())
            continue;
        const SliceRecord& x = *kv.second;
        const SliceRecord& y = *it->second;
        if (!x.judgeable || !y.judgeable)
            continue;

        long tx = 0, ty = 0;
        for (int v : x.skinVertexCounts)
            tx += v;
        for (int v : y.skinVertexCounts)
            ty += v;
        if (tx + ty == 0)
            continue;
        if (x.cut == y.cut) {
            dice.push_back(1.0);
            continue;
        }

        const size_t common = std::min(x.skinVertexCounts.size(), y.skinVertexCounts.size());
        long overlap = 0;
        for (size_t i = 0; i < common; ++i)
            overlap += std::min(x.skinVertexCounts[i], y.skinVertexCounts[i]);
        dice.push_back(
            std::min(2.0 * static_cast<double>(overlap) / static_cast<double>(tx + ty), 1.0));
    }
    if (nPairs)
        *nPairs = static_cast<int>(dice.size());
    return meanOf(dice);
}

// Best finite score; ascending betas + strict comparison = smaller beta on ties.
void pickBest(Selection& sel, bool maximize, const char* reasonIfNone) {
    double best = 0.0;
    for (const auto& row : sel.table) {
        if (std::isnan(row.score))
            continue;
        if (!sel.selected || (maximize ? row.score > best : row.score < best)) {
            sel.selected = true;
            sel.beta = row.beta;
            best = row.score;
        }
    }
    if (sel.selected)
        sel.scoreAtSelected = best;
    else
        sel.reason = reasonIfNone;
}

// Plateau entry: smallest k (not the last candidate) such that ok[j] holds
// for every adjacent pair j >= k.
void pickPlateau(Selection& sel, const std::vector<bool>& pairOk, const char* reasonIfNone) {
    const int nPairs = static_cast<int>(pairOk.size());
    for (int k = 0; k < nPairs; ++k) {
        bool all = true;
        for (int j = k; j < nPairs; ++j)
            if (!pairOk[j]) {
                all = false;
                break;
            }
        if (all) {
            sel.selected = true;
            sel.beta = sel.table[k].beta;
            sel.scoreAtSelected = sel.table[k].score;
            return;
        }
    }
    sel.reason = reasonIfNone;
}

void ruleAgreementPlateau(const Context& ctx, Selection& sel, double threshold) {
    const int n = static_cast<int>(ctx.cands.size());
    std::vector<bool> ok;
    for (int j = 0; j + 1 < n; ++j) {
        int used = 0;
        const double d = agreement(ctx, ctx.cands[j], ctx.cands[j + 1], &used);
        sel.table[j].score = d;
        sel.table[j].nUsed = used;
        ok.push_back(d >= threshold);  // NaN -> false
    }
    sel.diagnostics.emplace_back("threshold", threshold);
    pickPlateau(sel, ok, "no candidate from which every adjacent pair reaches the threshold");
}

void ruleA2(const Context& ctx, Selection& sel) {
    const int n = static_cast<int>(ctx.cands.size());
    // Pairwise agreement, computed once per unordered pair (smaller beta first).
    std::map<std::pair<int, int>, double> pairD;
    const double window = ctx.query.params.a2Window + kWindowSlack;
    for (int i = 0; i < n; ++i) {
        std::vector<double> vals;
        for (int c = 0; c < n; ++c) {
            if (c == i || std::abs(ctx.cands[c] - ctx.cands[i]) > window)
                continue;
            const std::pair<int, int> key(std::min(i, c), std::max(i, c));
            auto it = pairD.find(key);
            if (it == pairD.end())
                it = pairD.emplace(key, agreement(ctx, ctx.cands[key.first], ctx.cands[key.second]))
                         .first;
            vals.push_back(it->second);
        }
        sel.table[i].nUsed = static_cast<int>(vals.size());  // betas in the window
        sel.table[i].score = meanOf(vals);                   // NaN member -> NaN
    }
    pickBest(sel, true, "no candidate has another candidate within the window");
}

void ruleA3(const Context& ctx, Selection& sel) {
    const int n = static_cast<int>(ctx.cands.size());
    const double window = ctx.query.params.a3Window + kWindowSlack;
    for (int i = 0; i < n; ++i) {
        std::vector<const SliceMap*> neighbours;
        for (int c = 0; c < n; ++c) {
            const double dist = std::abs(ctx.cands[c] - ctx.cands[i]);
            if (dist > 1e-9 && dist <= window)
                neighbours.push_back(&ctx.at(ctx.cands[c]));
        }
        int total = 0, stable = 0;
        for (const auto& kv : ctx.at(ctx.cands[i])) {
            const SliceRecord& r = *kv.second;
            if (!r.judgeable)
                continue;
            ++total;
            bool same = true;
            for (const SliceMap* nb : neighbours) {
                const auto it = nb->find(kv.first);
                if (it == nb->end() || !it->second->judgeable || it->second->cut != r.cut) {
                    same = false;
                    break;
                }
            }
            if (same)
                ++stable;
        }
        sel.table[i].nUsed = total;
        sel.table[i].score = total > 0 ? static_cast<double>(stable) / total : kNaN;
    }
    pickBest(sel, true, "no judgeable slice at any candidate beta");
}

// Median over the valid records of a per-record value (NaN values dropped).
template <typename Getter>
void ruleMedian(const Context& ctx, Selection& sel, Getter value, bool maximize,
                const char* reasonIfNone) {
    for (size_t i = 0; i < ctx.cands.size(); ++i) {
        std::vector<double> vals;
        for (const auto& kv : ctx.at(ctx.cands[i])) {
            const double v = value(*kv.second);
            if (!std::isnan(v))
                vals.push_back(v);
        }
        sel.table[i].nUsed = static_cast<int>(vals.size());
        sel.table[i].score = median(vals);
    }
    pickBest(sel, maximize, reasonIfNone);
}

void ruleB4(const Context& ctx, Selection& sel) {
    for (size_t i = 0; i < ctx.cands.size(); ++i) {
        int total = 0, noChange = 0;
        for (const auto& kv : ctx.at(ctx.cands[i])) {
            const SliceRecord& r = *kv.second;
            ++total;
            // A slice that could not be judged has no changepoint either.
            if (!(r.judgeable && r.changepointFound))
                ++noChange;
        }
        sel.table[i].nUsed = total;
        sel.table[i].score = total > 0 ? static_cast<double>(noChange) / total : kNaN;
    }
    pickBest(sel, false, "no valid slice at any candidate beta");
}

double areaOf(const SliceMap& m, int slice) {
    const auto it = m.find(slice);
    if (it == m.end() || !it->second->evaluated)
        return kNaN;
    return it->second->area;
}

void ruleC1(const Context& ctx, Selection& sel) {
    const Params& p = ctx.query.params;
    double refBeta = p.c1ReferenceBeta;
    if (std::isnan(refBeta))
        refBeta = ctx.grid.back();
    else {
        // snap to a beta that is present
        bool found = false;
        for (double b : ctx.grid)
            if (std::abs(b - refBeta) <= 1e-9) {
                refBeta = b;
                found = true;
                break;
            }
        if (!found) {
            sel.reason = "reference beta not present in the records";
            return;
        }
    }
    sel.diagnostics.emplace_back("reference_beta", refBeta);
    sel.diagnostics.emplace_back("area_fraction", p.c1AreaFraction);
    sel.diagnostics.emplace_back("percentile", p.c1Percentile);

    // Slices: every valid slice present at any beta.
    std::set<int> slices;
    for (const auto& kv : ctx.byBeta)
        for (const auto& sv : kv.second)
            slices.insert(sv.first);

    // Per slice: first beta (over ALL betas present, not only candidates)
    // whose area reaches the fraction of the reference area.
    const SliceMap& refMap = ctx.at(refBeta);
    std::vector<double> first;
    for (int s : slices) {
        const double target = p.c1AreaFraction * areaOf(refMap, s);
        for (double b : ctx.grid) {
            if (areaOf(ctx.at(b), s) >= target) {  // NaN on either side -> false
                first.push_back(b);
                break;
            }
        }
    }
    const double v = percentile(first, p.c1Percentile);
    sel.diagnostics.emplace_back("n_slices", static_cast<double>(slices.size()));
    sel.diagnostics.emplace_back("n_slices_merged_at_some_beta", static_cast<double>(first.size()));
    sel.diagnostics.emplace_back("percentile_of_first_beta", v);

    for (auto& row : sel.table) {
        int merged = 0;
        for (double f : first)
            if (f <= row.beta + 1e-9)
                ++merged;
        row.nUsed = static_cast<int>(slices.size());
        row.score = slices.empty() ? kNaN : static_cast<double>(merged) / slices.size();
    }

    if (std::isnan(v)) {
        sel.reason = "no slice reaches the area fraction at any beta";
        return;
    }
    for (const auto& row : sel.table) {
        if (row.beta >= v - 1e-9) {  // round up onto the candidate grid
            sel.selected = true;
            sel.beta = row.beta;
            sel.scoreAtSelected = row.score;
            return;
        }
    }
    sel.reason = "percentile of the per-slice merge beta lies above every candidate";
}

void ruleC2(const Context& ctx, Selection& sel) {
    const int n = static_cast<int>(ctx.cands.size());
    std::vector<bool> ok;
    for (int j = 0; j + 1 < n; ++j) {
        const SliceMap& A = ctx.at(ctx.cands[j]);
        const SliceMap& C = ctx.at(ctx.cands[j + 1]);
        std::set<int> slices;
        for (const auto& kv : A)
            slices.insert(kv.first);
        for (const auto& kv : C)
            slices.insert(kv.first);
        std::vector<double> rel;
        for (int s : slices) {
            const double r = std::abs(areaOf(C, s) / areaOf(A, s) - 1.0);
            if (!std::isnan(r))
                rel.push_back(r);  // 0/0 or a missing side is dropped; x/0 = inf stays
        }
        const double med = median(rel);
        sel.table[j].score = med;
        sel.table[j].nUsed = static_cast<int>(rel.size());
        ok.push_back(med <= ctx.query.params.c2Epsilon);  // NaN -> false
    }
    sel.diagnostics.emplace_back("epsilon", ctx.query.params.c2Epsilon);
    pickPlateau(sel, ok, "no candidate from which every adjacent area change stays within epsilon");
}

// Median of an integer per-record quantity over the evaluated valid records.
template <typename Getter>
double medianInt(const SliceMap& m, Getter value, int* nUsed = nullptr) {
    std::vector<double> vals;
    for (const auto& kv : m)
        if (kv.second->evaluated)
            vals.push_back(static_cast<double>(value(*kv.second)));
    if (nUsed)
        *nUsed = static_cast<int>(vals.size());
    return median(vals);
}

void ruleC3a(const Context& ctx, Selection& sel) {
    const int n = static_cast<int>(ctx.cands.size());
    std::vector<std::pair<double, double>> pairs(n);
    for (int i = 0; i < n; ++i) {
        const SliceMap& m = ctx.at(ctx.cands[i]);
        int used = 0;
        pairs[i].first = medianInt(m, [](const SliceRecord& r) { return r.nComponents; }, &used);
        pairs[i].second = medianInt(m, [](const SliceRecord& r) { return r.nInnerLoops; });
        sel.table[i].score = pairs[i].first;
        sel.table[i].nUsed = used;
        sel.diagnostics.emplace_back("median_n_inner_loops@" + std::to_string(i), pairs[i].second);
    }
    // Longest run of equal consecutive pairs; the first longest run wins.
    int bestLen = 0, bestStart = 0, runStart = 0, runLen = 0;
    for (int i = 0; i < n; ++i) {
        const bool same = i > 0 && pairs[i].first == pairs[i - 1].first &&
                          pairs[i].second == pairs[i - 1].second;  // NaN never equal
        if (same)
            ++runLen;
        else {
            runStart = i;
            runLen = 1;
        }
        if (runLen > bestLen) {
            bestLen = runLen;
            bestStart = runStart;
        }
    }
    sel.diagnostics.emplace_back("longest_run_length", static_cast<double>(bestLen));
    sel.selected = true;
    sel.beta = sel.table[bestStart].beta;
    sel.scoreAtSelected = sel.table[bestStart].score;
}

void ruleC4(const Context& ctx, Selection& sel) {
    const int n = static_cast<int>(ctx.cands.size());
    for (int i = 0; i < n; ++i) {
        int used = 0;
        sel.table[i].score =
            medianInt(ctx.at(ctx.cands[i]), [](const SliceRecord& r) { return r.nShells; }, &used);
        sel.table[i].nUsed = used;
    }
    std::vector<bool> ok;
    for (int j = 0; j + 1 < n; ++j)
        ok.push_back(std::abs(sel.table[j + 1].score - sel.table[j].score) <=
                     ctx.query.params.c4MaxChange);  // NaN -> false
    sel.diagnostics.emplace_back("max_change", ctx.query.params.c4MaxChange);
    pickPlateau(sel, ok,
                "no candidate from which the median shell count stays within the allowed change");
}

}  // namespace

Selection select(const std::vector<SliceRecord>& records, const Query& query) {
    Selection sel;
    sel.rule = query.rule;

    // ---- Group the valid records -------------------------------------------
    Context ctx{query, {}, {}, {}, true, true};
    for (const auto& rec : records) {
        if (query.restrictSlices && !query.validSlices.count(rec.sliceIndex))
            continue;
        ctx.byBeta[rec.beta][rec.sliceIndex] = &rec;
    }
    for (const auto& kv : ctx.byBeta) {
        const double beta = kv.first;
        ctx.grid.push_back(beta);
        if (query.restrictBetas && (beta < query.betaMin - 1e-9 || beta > query.betaMax + 1e-9))
            continue;
        ctx.cands.push_back(beta);

        // Common columns of the score table
        BetaScore row;
        row.beta = beta;
        for (const auto& sv : kv.second) {
            const SliceRecord& r = *sv.second;
            ++row.nValid;
            if (r.evaluated && r.nInnerLoops < 0)
                ctx.innerLoopsComplete = false;
            if (!r.judgeable)
                continue;
            ++row.nJudgeable;
            if (r.cut >= 2)
                ++row.nCutGe2;
            if (!r.hasShellData)
                ctx.shellDataComplete = false;
        }
        if (row.nValid > 0)
            row.judgeableShare = static_cast<double>(row.nJudgeable) / row.nValid;
        if (row.nJudgeable > 0)
            row.cutGe2Share = static_cast<double>(row.nCutGe2) / row.nJudgeable;
        sel.table.push_back(row);
    }
    if (ctx.cands.empty()) {
        sel.reason = "no candidate beta";
        return sel;
    }

    const bool needsShells =
        (query.rule == Rule::A1 || query.rule == Rule::A2 || query.rule == Rule::A4 ||
         query.rule == Rule::B1 || query.rule == Rule::B3);
    if (needsShells && !ctx.shellDataComplete) {
        sel.reason = "per-shell data not provided for every judgeable slice";
        return sel;
    }
    const bool needsPair = (query.rule == Rule::A1 || query.rule == Rule::A4 ||
                            query.rule == Rule::C2 || query.rule == Rule::C4);
    if (needsPair && ctx.cands.size() < 2) {
        sel.reason = "fewer than two candidate betas";
        return sel;
    }

    switch (query.rule) {
        case Rule::A1:
            ruleAgreementPlateau(ctx, sel, query.params.a1Threshold);
            break;
        case Rule::A4:
            ruleAgreementPlateau(ctx, sel, query.params.a4Threshold);
            break;
        case Rule::A2:
            ruleA2(ctx, sel);
            break;
        case Rule::A3:
            ruleA3(ctx, sel);
            break;
        case Rule::B1:
            ruleMedian(
                ctx, sel, [](const SliceRecord& r) { return r.dBic; }, true,
                "no slice with a defined BIC difference");
            break;
        case Rule::B2:
            ruleMedian(
                ctx, sel, [](const SliceRecord& r) { return r.judgeable ? r.b2Contrast : kNaN; },
                true, "no slice with a defined separation contrast");
            break;
        case Rule::B3:
            ruleMedian(
                ctx, sel, [](const SliceRecord& r) { return r.bodyLogDet; }, false,
                "no slice with a defined body covariance determinant");
            break;
        case Rule::B4:
            ruleB4(ctx, sel);
            break;
        case Rule::C1:
            ruleC1(ctx, sel);
            break;
        case Rule::C2:
            ruleC2(ctx, sel);
            break;
        case Rule::C3a:
            if (!ctx.innerLoopsComplete)
                sel.reason = "n_inner_loops not available in the records";
            else
                ruleC3a(ctx, sel);
            break;
        case Rule::C4:
            ruleC4(ctx, sel);
            break;
    }
    return sel;
}

}  // namespace BetaSelection
