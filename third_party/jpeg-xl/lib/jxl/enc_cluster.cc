// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_cluster.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <tuple>

#include "lib/jxl/base/status.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_cluster.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/fast_math-inl.h"
#include "lib/jxl/enc_ans.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Eq;
using hwy::HWY_NAMESPACE::IfThenZeroElse;

template <class V>
V Entropy(V count, V inv_total, V total) {
  const HWY_CAPPED(float, Histogram::kRounding) d;
  const auto zero = Set(d, 0.0f);
  // TODO(eustas): why (0 - x) instead of Neg(x)?
  return IfThenZeroElse(
      Eq(count, total),
      Sub(zero, Mul(count, FastLog2f(d, Mul(inv_total, count)))));
}

void HistogramEntropy(const Histogram& a) {
  a.entropy_ = 0.0f;
  if (a.total_count_ == 0) return;

  const HWY_CAPPED(float, Histogram::kRounding) df;
  const HWY_CAPPED(int32_t, Histogram::kRounding) di;

  const auto inv_tot = Set(df, 1.0f / a.total_count_);
  auto entropy_lanes = Zero(df);
  auto total = Set(df, a.total_count_);

  for (size_t i = 0; i < a.data_.size(); i += Lanes(di)) {
    const auto counts = LoadU(di, &a.data_[i]);
    entropy_lanes =
        Add(entropy_lanes, Entropy(ConvertTo(df, counts), inv_tot, total));
  }
  a.entropy_ += GetLane(SumOfLanes(df, entropy_lanes));
}

float HistogramDistance(const Histogram& a, const Histogram& b) {
  if (a.total_count_ == 0 || b.total_count_ == 0) return 0;

  const HWY_CAPPED(float, Histogram::kRounding) df;
  const HWY_CAPPED(int32_t, Histogram::kRounding) di;

  const auto inv_tot = Set(df, 1.0f / (a.total_count_ + b.total_count_));
  auto distance_lanes = Zero(df);
  auto total = Set(df, a.total_count_ + b.total_count_);

  for (size_t i = 0; i < std::max(a.data_.size(), b.data_.size());
       i += Lanes(di)) {
    const auto a_counts =
        a.data_.size() > i ? LoadU(di, &a.data_[i]) : Zero(di);
    const auto b_counts =
        b.data_.size() > i ? LoadU(di, &b.data_[i]) : Zero(di);
    const auto counts = ConvertTo(df, Add(a_counts, b_counts));
    distance_lanes = Add(distance_lanes, Entropy(counts, inv_tot, total));
  }
  const float total_distance = GetLane(SumOfLanes(df, distance_lanes));
  return total_distance - a.entropy_ - b.entropy_;
}

constexpr const float kInfinity = std::numeric_limits<float>::infinity();

float HistogramKLDivergence(const Histogram& actual, const Histogram& coding) {
  if (actual.total_count_ == 0) return 0;
  if (coding.total_count_ == 0) return kInfinity;

  const HWY_CAPPED(float, Histogram::kRounding) df;
  const HWY_CAPPED(int32_t, Histogram::kRounding) di;

  const auto coding_inv = Set(df, 1.0f / coding.total_count_);
  auto cost_lanes = Zero(df);

  for (size_t i = 0; i < actual.data_.size(); i += Lanes(di)) {
    const auto counts = LoadU(di, &actual.data_[i]);
    const auto coding_counts =
        coding.data_.size() > i ? LoadU(di, &coding.data_[i]) : Zero(di);
    const auto coding_probs = Mul(ConvertTo(df, coding_counts), coding_inv);
    const auto neg_coding_cost = BitCast(
        df,
        IfThenZeroElse(Eq(counts, Zero(di)),
                       IfThenElse(Eq(coding_counts, Zero(di)),
                                  BitCast(di, Set(df, -kInfinity)),
                                  BitCast(di, FastLog2f(df, coding_probs)))));
    cost_lanes = NegMulAdd(ConvertTo(df, counts), neg_coding_cost, cost_lanes);
  }
  const float total_cost = GetLane(SumOfLanes(df, cost_lanes));
  return total_cost - actual.entropy_;
}

// First step of a k-means clustering with a fancy distance metric.
Status FastClusterHistograms(const std::vector<Histogram>& in,
                             size_t max_histograms, std::vector<Histogram>* out,
                             std::vector<uint32_t>* histogram_symbols) {
  const size_t prev_histograms = out->size();
  out->reserve(max_histograms);
  histogram_symbols->clear();
  histogram_symbols->resize(in.size(), max_histograms);

  std::vector<float> dists(in.size(), std::numeric_limits<float>::max());
  size_t largest_idx = 0;
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i].total_count_ == 0) {
      (*histogram_symbols)[i] = 0;
      dists[i] = 0.0f;
      continue;
    }
    HistogramEntropy(in[i]);
    if (in[i].total_count_ > in[largest_idx].total_count_) {
      largest_idx = i;
    }
  }

  if (prev_histograms > 0) {
    for (size_t j = 0; j < prev_histograms; ++j) {
      HistogramEntropy((*out)[j]);
    }
    for (size_t i = 0; i < in.size(); i++) {
      if (dists[i] == 0.0f) continue;
      for (size_t j = 0; j < prev_histograms; ++j) {
        dists[i] = std::min(HistogramKLDivergence(in[i], (*out)[j]), dists[i]);
      }
    }
    auto max_dist = std::max_element(dists.begin(), dists.end());
    if (*max_dist > 0.0f) {
      largest_idx = max_dist - dists.begin();
    }
  }

  constexpr float kMinDistanceForDistinct = 48.0f;
  while (out->size() < max_histograms) {
    (*histogram_symbols)[largest_idx] = out->size();
    out->push_back(in[largest_idx]);
    dists[largest_idx] = 0.0f;
    largest_idx = 0;
    for (size_t i = 0; i < in.size(); i++) {
      if (dists[i] == 0.0f) continue;
      dists[i] = std::min(HistogramDistance(in[i], out->back()), dists[i]);
      if (dists[i] > dists[largest_idx]) largest_idx = i;
    }
    if (dists[largest_idx] < kMinDistanceForDistinct) break;
  }

  for (size_t i = 0; i < in.size(); i++) {
    if ((*histogram_symbols)[i] != max_histograms) continue;
    size_t best = 0;
    float best_dist = std::numeric_limits<float>::max();
    for (size_t j = 0; j < out->size(); j++) {
      float dist = j < prev_histograms ? HistogramKLDivergence(in[i], (*out)[j])
                                       : HistogramDistance(in[i], (*out)[j]);
      if (dist < best_dist) {
        best = j;
        best_dist = dist;
      }
    }
    JXL_ENSURE(best_dist < std::numeric_limits<float>::max());
    if (best >= prev_histograms) {
      (*out)[best].AddHistogram(in[i]);
      HistogramEntropy((*out)[best]);
    }
    (*histogram_symbols)[i] = best;
  }
  return true;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(FastClusterHistograms);  // Local function
HWY_EXPORT(HistogramEntropy);       // Local function

StatusOr<float> Histogram::PopulationCost() const {
  return ANSPopulationCost(data_.data(), data_.size());
}

float Histogram::ShannonEntropy() const {
  HWY_DYNAMIC_DISPATCH(HistogramEntropy)(*this);
  return entropy_;
}

namespace {
// -----------------------------------------------------------------------------
// Histogram refinement

// Reorder histograms in *out so that the new symbols in *symbols come in
// increasing order.
void HistogramReindex(std::vector<Histogram>* out, size_t prev_histograms,
                      std::vector<uint32_t>* symbols) {
  std::vector<Histogram> tmp(*out);
  std::map<int, int> new_index;
  for (size_t i = 0; i < prev_histograms; ++i) {
    new_index[i] = i;
  }
  int next_index = prev_histograms;
  for (uint32_t symbol : *symbols) {
    if (new_index.find(symbol) == new_index.end()) {
      new_index[symbol] = next_index;
      (*out)[next_index] = tmp[symbol];
      ++next_index;
    }
  }
  out->resize(next_index);
  for (uint32_t& symbol : *symbols) {
    symbol = new_index[symbol];
  }
}

}  // namespace

// Clusters similar histograms in 'in' together, the selected histograms are
// placed in 'out', and for each index in 'in', *histogram_symbols will
// indicate which of the 'out' histograms is the best approximation.
Status ClusterHistograms(const HistogramParams& params,
                         const std::vector<Histogram>& in,
                         size_t max_histograms, std::vector<Histogram>* out,
                         std::vector<uint32_t>* histogram_symbols) {
  size_t prev_histograms = out->size();
  max_histograms = std::min(max_histograms, params.max_histograms);
  max_histograms = std::min(max_histograms, in.size());
  if (params.clustering == HistogramParams::ClusteringType::kFastest) {
    max_histograms = std::min(max_histograms, static_cast<size_t>(4));
  }

  JXL_RETURN_IF_ERROR(HWY_DYNAMIC_DISPATCH(FastClusterHistograms)(
      in, prev_histograms + max_histograms, out, histogram_symbols));

  if (prev_histograms == 0 &&
      params.clustering == HistogramParams::ClusteringType::kBest) {
    for (auto& histo : *out) {
      JXL_ASSIGN_OR_RETURN(
          histo.entropy_,
          ANSPopulationCost(histo.data_.data(), histo.data_.size()));
    }
    uint32_t next_version = 2;
    std::vector<uint32_t> version(out->size(), 1);
    std::vector<uint32_t> renumbering(out->size());
    std::iota(renumbering.begin(), renumbering.end(), 0);

    // Try to pair up clusters if doing so reduces the total cost.

    struct HistogramPair {
      // validity of a pair: p.version == max(version[i], version[j])
      float cost;
      uint32_t first;
      uint32_t second;
      uint32_t version;
      // We use > because priority queues sort in *decreasing* order, but we
      // want lower cost elements to appear first.
      bool operator<(const HistogramPair& other) const {
        return std::make_tuple(cost, first, second, version) >
               std::make_tuple(other.cost, other.first, other.second,
                               other.version);
      }
    };

    // Create list of all pairs by increasing merging cost.
    std::priority_queue<HistogramPair> pairs_to_merge;
    for (uint32_t i = 0; i < out->size(); i++) {
      for (uint32_t j = i + 1; j < out->size(); j++) {
        Histogram histo;
        histo.AddHistogram((*out)[i]);
        histo.AddHistogram((*out)[j]);
        JXL_ASSIGN_OR_RETURN(float cost, ANSPopulationCost(histo.data_.data(),
                                                           histo.data_.size()));
        cost -= (*out)[i].entropy_ + (*out)[j].entropy_;
        // Avoid enqueueing pairs that are not advantageous to merge.
        if (cost >= 0) continue;
        pairs_to_merge.push(
            HistogramPair{cost, i, j, std::max(version[i], version[j])});
      }
    }

    // Merge the best pair to merge, add new pairs that get formed as a
    // consequence.
    while (!pairs_to_merge.empty()) {
      uint32_t first = pairs_to_merge.top().first;
      uint32_t second = pairs_to_merge.top().second;
      uint32_t ver = pairs_to_merge.top().version;
      pairs_to_merge.pop();
      if (ver != std::max(version[first], version[second]) ||
          version[first] == 0 || version[second] == 0) {
        continue;
      }
      (*out)[first].AddHistogram((*out)[second]);
      JXL_ASSIGN_OR_RETURN(float cost,
                           ANSPopulationCost((*out)[first].data_.data(),
                                             (*out)[first].data_.size()));
      (*out)[first].entropy_ = cost;
      for (uint32_t& item : renumbering) {
        if (item == second) {
          item = first;
        }
      }
      version[second] = 0;
      version[first] = next_version++;
      for (uint32_t j = 0; j < out->size(); j++) {
        if (j == first) continue;
        if (version[j] == 0) continue;
        Histogram histo;
        histo.AddHistogram((*out)[first]);
        histo.AddHistogram((*out)[j]);
        JXL_ASSIGN_OR_RETURN(float cost, ANSPopulationCost(histo.data_.data(),
                                                           histo.data_.size()));
        cost -= (*out)[first].entropy_ + (*out)[j].entropy_;
        // Avoid enqueueing pairs that are not advantageous to merge.
        if (cost >= 0) continue;
        pairs_to_merge.push(
            HistogramPair{cost, std::min(first, j), std::max(first, j),
                          std::max(version[first], version[j])});
      }
    }
    std::vector<uint32_t> reverse_renumbering(out->size(), -1);
    size_t num_alive = 0;
    for (size_t i = 0; i < out->size(); i++) {
      if (version[i] == 0) continue;
      (*out)[num_alive++] = (*out)[i];
      reverse_renumbering[i] = num_alive - 1;
    }
    out->resize(num_alive);
    for (uint32_t& item : *histogram_symbols) {
      item = reverse_renumbering[renumbering[item]];
    }
  }

  // Convert the context map to a canonical form.
  HistogramReindex(out, prev_histograms, histogram_symbols);
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
