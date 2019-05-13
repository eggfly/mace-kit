// Copyright 2019 The MACE-KIT Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <cmath>
#include <algorithm>
#include <functional>
#include "macekit/ssd_bbox.h"

namespace mace_kit {
namespace util {

SSDBbox::SSDBbox() {

}

SSDBbox::~SSDBbox() {

}

// `feature_localization`: regressed output localization. Its size
// should be `feature_size` x anchor_count x 4 (cy, cx, h, w).
//
// This implies decoding formula:
// g(cx) = d(cx) + g^(cx) * d(w) * prior_scaling(cx)
// g(cy) = d(cy) + g^(cy) * d(h) * prior_scaling(cy)
// g(w) = exp[g^(w) * d(w) * prior_scaling(w)]
// g(h) = exp[g^(h) * d(h) * prior_scaling(h)]
//
// So final decoded bbox location should be:
// [g(cy) - g(h) / 2, g(cx) - g(w) / 2, g(cy) + g(h) / 2, g(cx) + g(w) / 2]
//
void SSDBbox::Decode(float *feature_localization,
                     const std::vector<int64_t> &feature_shape,
                     const std::vector<int64_t> &img_shape,
                     int step,
                     const std::vector<std::vector<float>> &anchor_shapes,
                     const std::vector<float> &prior_scaling,
                     float offset) {
  float step_scale = step == 0 ? 1.0f * img_shape[0] / feature_shape[0] : step;
  int anchor_count = anchor_shapes.size();

  for (int64_t h = 0; h < feature_shape[0]; h++) {
    for (int64_t w = 0; w < feature_shape[1]; w++) {
      float *feature_data =
          feature_localization + (h * feature_shape[1] + w) * anchor_count * 4;

      for (int anchor_idx = 0; anchor_idx < anchor_count; anchor_idx++) {
        float dh = anchor_shapes[anchor_idx][0];
        float dw = anchor_shapes[anchor_idx][1];
        float dcy = (h + offset) * step_scale / img_shape[0];
        float dcx = (w + offset) * step_scale / img_shape[1];

        float gcy = dcy + feature_data[0] * dh * prior_scaling[0];  // cy
        float gcx = dcx + feature_data[1] * dw * prior_scaling[1];  // cx
        float gh = exp(feature_data[2] * dh * prior_scaling[2]);   // h
        float gw = exp(feature_data[3] * dw * prior_scaling[3]);   // w

        feature_data[0] = std::max(0.f, gcy - gh / 2);
        feature_data[1] = std::max(0.f, gcx - gw / 2);
        feature_data[2] = std::min(1.f, gcy + gh / 2);
        feature_data[3] = std::min(1.f, gcx + gw / 2);

        feature_data += 4;
      }  // anchor_idx
    }  // w
  }  // h
}

void SSDBbox::GetAnchorsShape(int64_t img_height,
                              int64_t img_width,
                              int64_t min_size,
                              int64_t max_size,
                              const std::vector<float> &anchor_ratios,
                              std::vector<std::vector<float>> *anchor_shapes) {
  anchor_shapes->push_back({1.0f * min_size / img_height,
                            1.0f * min_size / img_width});
  if (max_size > min_size) {
    anchor_shapes->push_back({static_cast<float>(
                                  sqrt(1.0f * min_size * max_size)
                                      / img_height),
                              static_cast<float>(
                                  sqrt(1.0f * min_size * max_size)
                                      / img_width)});
  }

  for (float ratio : anchor_ratios) {
    float sqrt_ratio = sqrt(ratio);
    anchor_shapes->push_back({1.0f * min_size / img_height / sqrt_ratio,
                              1.0f * min_size / img_width * sqrt_ratio});
  }
}

void SSDBbox::SelectTopAndNMS(const float *scores,
                              const float *localization,
                              int64_t anchor_count,
                              std::vector<float> *output_scores,
                              std::vector<float> *output_localization,
                              int64_t top_k,
                              float nms_threshold) {
  std::vector<std::pair<float, int>> scores_with_indices(anchor_count);
  for (int i = 0; i < anchor_count; i++) {
    scores_with_indices[i] = {scores[i], i};
  }
  std::sort(scores_with_indices.begin(),
            scores_with_indices.end(),
            [](const std::pair<float, int> &lhs,
               const std::pair<float, int> &rhs) {
              return lhs.first > rhs.first
                  || (lhs.first == rhs.first && lhs.second < rhs.second);
            });
  std::vector<int> indices(top_k);
  for (int i = 0; i < top_k; i++) {
    indices[i] = scores_with_indices[i].second;
  }

  output_scores->push_back(scores[indices[0]]);
  output_localization->insert(output_localization->end(),
                              localization,
                              localization + 4);

  for (int i = 1; i < top_k; i++) {
    bool retain = true;

    for (int j = 0; j < i; j++) {
      float iou = CalJaccard(localization + indices[i] * 4,
                             localization + indices[j] * 4);
      if (iou > 0.45) {
        retain = false;
        break;
      }
    }  // j

    if (retain) {
      output_scores->push_back(scores[indices[i]]);
      output_localization->insert(output_localization->end(),
                                  localization + indices[i] * 4,
                                  localization + indices[i] * 4 + 4);
    }
  }  // i
}

// xmin, ymin, xmax, ymax
float SSDBbox::CalJaccard(const float *lhs, const float *rhs) {
  float intersection =
      std::max(0.f, std::min(lhs[2], rhs[2]) - std::max(lhs[0], rhs[0]))
          * std::max(0.f, std::min(lhs[3], rhs[3]) - std::max(lhs[1], rhs[1]));
  float unionarea = (lhs[2] - lhs[0]) * (lhs[3] - lhs[1])
      + (rhs[2] - rhs[0]) * (rhs[3] - rhs[1]) - intersection;
  return intersection / unionarea;
}

}  // namespace util
}  // namespace mace_kit