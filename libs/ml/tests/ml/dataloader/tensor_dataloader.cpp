//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "core/serializers/main_serializer.hpp"
#include "math/base_types.hpp"
#include "math/tensor.hpp"
#include "ml/dataloaders/tensor_dataloader.hpp"
#include "vectorise/fixed_point/fixed_point.hpp"

#include "gtest/gtest.h"

using namespace fetch::ml;
using namespace fetch::ml::dataloaders;

template <typename T>
class TensorDataloaderTest : public ::testing::Test
{
};

using MyTypes = ::testing::Types<fetch::math::Tensor<int>, fetch::math::Tensor<float>,
                                 fetch::math::Tensor<double>,
                                 fetch::math::Tensor<fetch::fixed_point::FixedPoint<16, 16>>,
                                 fetch::math::Tensor<fetch::fixed_point::FixedPoint<32, 32>>>;
TYPED_TEST_CASE(TensorDataloaderTest, MyTypes);

TYPED_TEST(TensorDataloaderTest, serialize_tensor_dataloader)
{
  TypeParam label_tensor = TypeParam::UniformRandom(4);
  TypeParam data1_tensor = TypeParam::UniformRandom(24);
  label_tensor.Reshape({1, 4});
  data1_tensor.Reshape({2, 3, 4});

  // generate a plausible tensor data loader
  auto tdl = fetch::ml::dataloaders::TensorDataLoader<TypeParam, TypeParam>(label_tensor.shape(),
                                                                            {data1_tensor.shape()});

  // add some data
  tdl.AddData(data1_tensor, label_tensor);

  fetch::serializers::MsgPackSerializer b;
  b << tdl;

  b.seek(0);

  // initialise a new tensor dataloader with the wrong shape parameters
  // these will get updated by deserialisation
  fetch::ml::dataloaders::TensorDataLoader<TypeParam, TypeParam> tdl_2({1, 1}, {{1, 1}});
  tdl_2.SetTestRatio(0.5);

  b >> tdl_2;

  EXPECT_EQ(tdl.Size(), tdl_2.Size());
  EXPECT_EQ(tdl.IsDone(), tdl_2.IsDone());
  EXPECT_EQ(tdl.GetNext(), tdl_2.GetNext());

  // add some new data
  label_tensor = TypeParam::UniformRandom(4);
  data1_tensor = TypeParam::UniformRandom(24);
  label_tensor.Reshape({1, 4});
  data1_tensor.Reshape({2, 3, 4});
  EXPECT_EQ(tdl.AddData(data1_tensor, label_tensor), tdl_2.AddData(data1_tensor, label_tensor));

  EXPECT_EQ(tdl.Size(), tdl_2.Size());
  EXPECT_EQ(tdl.IsDone(), tdl_2.IsDone());
  EXPECT_EQ(tdl.GetNext(), tdl_2.GetNext());
}

TYPED_TEST(TensorDataloaderTest, test_validation_splitting_dataloader_test)
{
  TypeParam label_tensor = TypeParam::UniformRandom(4);
  TypeParam data1_tensor = TypeParam::UniformRandom(24);
  label_tensor.Reshape({1, 1});
  data1_tensor.Reshape({2, 3, 1});

  // generate a plausible tensor data loader
  auto tdl = fetch::ml::dataloaders::TensorDataLoader<TypeParam, TypeParam>(label_tensor.shape(),
                                                                            {data1_tensor.shape()});
  tdl.SetTestRatio(0.1f);
  tdl.SetValidationRatio(0.1f);

  // add some data
  tdl.AddData(data1_tensor, label_tensor);

  EXPECT_EQ(tdl.Size(), 1);
  EXPECT_THROW(tdl.SetMode(DataLoaderMode::TEST), std::runtime_error);
  EXPECT_THROW(tdl.SetMode(DataLoaderMode::VALIDATE), std::runtime_error);
}

TYPED_TEST(TensorDataloaderTest, prepare_batch_test)
{
  using SizeType = fetch::math::SizeType;

  SizeType feature_size_1 = 2;
  SizeType feature_size_2 = 3;
  SizeType batch_size     = 2;
  SizeType n_data         = 10;

  TypeParam label_tensor = TypeParam::UniformRandom(n_data);
  TypeParam data1_tensor = TypeParam::UniformRandom(feature_size_1 * feature_size_2 * n_data);
  label_tensor.Reshape({1, n_data});
  data1_tensor.Reshape({feature_size_1, feature_size_2, n_data});

  // generate a plausible tensor data loader
  auto tdl = fetch::ml::dataloaders::TensorDataLoader<TypeParam, TypeParam>(label_tensor.shape(),
                                                                            {data1_tensor.shape()});
  // add some data
  tdl.AddData(data1_tensor, label_tensor);

  bool is_done_set = false;
  auto batch       = tdl.PrepareBatch(batch_size, is_done_set).second;

  EXPECT_EQ(batch.at(0).shape(),
            std::vector<SizeType>({feature_size_1, feature_size_2, batch_size}));
}
