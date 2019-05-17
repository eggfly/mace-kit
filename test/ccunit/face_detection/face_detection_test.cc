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

#include "gtest/gtest.h"
#include "macekit/face_detection.h"

namespace mace_kit {

class FaceDetectionTest : public ::testing::Test {
 public:
  FaceDetectionTest() {
    FaceDetectionContext context;
    context.device_type = DeviceType::CPU;
    context.cpu_affinity_policy = CPUAffinityPolicy::AFFINITY_NONE;
    context.thread_count = 4;

    FaceDetection::Create(context, &face_detection_);
  }

  ~FaceDetectionTest() {
      delete face_detection_;
  }

 protected:
  FaceDetection *face_detection_;
};

TEST_F(FaceDetectionTest, TestDetect) {

}

}  // mace_kit