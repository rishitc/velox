/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "velox/common/base/tests/GTestUtils.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/functions/lib/aggregates/tests/utils/AggregationTestBase.h"

using namespace facebook::velox::exec;
using namespace facebook::velox::functions::aggregate::test;

namespace facebook::velox::aggregate::test {

namespace {

class SetUnionTest : public AggregationTestBase {
 protected:
  void SetUp() override {
    AggregationTestBase::SetUp();
    allowInputShuffle();
  }
};

TEST_F(SetUnionTest, global) {
  auto data = makeRowVector({
      makeArrayVector<int32_t>({
          {},
          {1, 2, 3},
          {1, 2},
          {2, 3, 4, 5},
          {6, 7},
      }),
  });

  auto expected = makeRowVector({
      makeArrayVector<int32_t>({
          {1, 2, 3, 4, 5, 6, 7},
      }),
  });

  testAggregations(
      {data}, {}, {"set_union(c0)"}, {"array_sort(a0)"}, {expected});

  // Null inputs.
  data = makeRowVector({
      makeArrayVectorFromJson<int32_t>({
          "[]",
          "[1, 2, null, 3]",
          "[1, 2]",
          "null",
          "[2, 3, 4, null, 5]",
          "[6, 7]",
      }),
  });

  expected = makeRowVector({
      makeArrayVectorFromJson<int32_t>({"[1, 2, 3, 4, 5, 6, 7, null]"}),
  });

  testAggregations(
      {data}, {}, {"set_union(c0)"}, {"array_sort(a0)"}, {expected});

  // All nulls arrays.
  data = makeRowVector({
      makeAllNullArrayVector(10, INTEGER()),
  });

  expected = makeRowVector({
      // Empty array: [].
      makeArrayVector<int32_t>({{}}),
  });

  testAggregations(
      {data}, {}, {"set_union(c0)"}, {"array_sort(a0)"}, {expected});

  // All nulls elements.
  data = makeRowVector({
      makeArrayVectorFromJson<int32_t>({
          "[]",
          "[null, null, null, null]",
          "[null, null, null]",
      }),
  });

  expected = makeRowVector({
      makeArrayVectorFromJson<int32_t>({"[null]"}),
  });

  testAggregations(
      {data}, {}, {"set_union(c0)"}, {"array_sort(a0)"}, {expected});
}

TEST_F(SetUnionTest, groupBy) {
  auto data = makeRowVector({
      makeFlatVector<int16_t>({1, 1, 2, 2, 2, 1, 2, 1, 2, 1}),
      makeArrayVector<int32_t>({
          {},
          {1, 2, 3}, // masked out
          {10, 20},
          {20, 30, 40},
          {10, 50}, // masked out
          {4, 2, 1, 5}, // masked out
          {60, 20},
          {5, 6},
          {}, // masked out
          {},
      }),
      makeFlatVector<bool>(
          {true, false, true, true, false, false, true, true, false, true}),
  });

  auto expected = makeRowVector({
      makeFlatVector<int16_t>({1, 2}),
      makeArrayVector<int32_t>({
          {1, 2, 3, 4, 5, 6},
          {10, 20, 30, 40, 50, 60},
      }),
  });

  testAggregations(
      {data, data, data},
      {"c0"},
      {"set_union(c1)"},
      {"c0", "array_sort(a0)"},
      {expected});

  expected = makeRowVector({
      makeFlatVector<int16_t>({1, 2}),
      makeArrayVector<int32_t>({
          {5, 6},
          {10, 20, 30, 40, 60},
      }),
  });

  testAggregations(
      {data, data, data},
      {"c0"},
      {"set_union(c1) filter (where c2)"},
      {"c0", "array_sort(a0)"},
      {expected});

  // Null inputs.
  data = makeRowVector({
      makeFlatVector<int16_t>({1, 1, 2, 2, 2, 1, 2, 1, 2, 1}),
      makeArrayVectorFromJson<int32_t>({
          "[]",
          "[1, 2, 3]",
          "[10, null, 20]",
          "[20, 30, 40, null, 50]",
          "null",
          "[4, 2, 1, 5]",
          "[60, null, 20]",
          "[null, 5, 6]",
          "[]",
          "null",
      }),
  });

  expected = makeRowVector({
      makeFlatVector<int16_t>({1, 2}),
      makeArrayVectorFromJson<int32_t>({
          "[1, 2, 3, 4, 5, 6, null]",
          "[10, 20, 30, 40, 50, 60, null]",
      }),
  });

  testAggregations(
      {data, data, data},
      {"c0"},
      {"set_union(c1)"},
      {"c0", "array_sort(a0)"},
      {expected});

  // All null arrays for one group.
  std::vector<RowVectorPtr> multiBatchData = {
      makeRowVector({
          makeFlatVector<int16_t>({1, 1, 2, 2, 2, 1, 2}),
          makeArrayVectorFromJson<int32_t>({
              "null",
              "null",
              "[]",
              "[1, 2]",
              "[1, 2, 3]",
              "null",
              "null",
          }),
      }),
      makeRowVector({
          makeFlatVector<int16_t>({3, 3, 3, 2, 3}),
          makeArrayVectorFromJson<int32_t>({
              "null",
              "null",
              "null",
              "[2, 4, 5]",
              "null",
          }),
      }),
  };

  expected = makeRowVector({
      makeFlatVector<int16_t>({1, 2, 3}),
      makeArrayVector<int32_t>({
          {}, // Empty array: [].
          {1, 2, 3, 4, 5},
          {}, // Empty array: [].
      }),
  });

  testAggregations(
      multiBatchData,
      {"c0"},
      {"set_union(c1)"},
      {"c0", "array_sort(a0)"},
      {expected});
}

} // namespace
} // namespace facebook::velox::aggregate::test
