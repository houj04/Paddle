/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/framework/data_device_transform.h"

namespace paddle::framework {

void TransDataDevice(const phi::DenseTensor &in,
                     const phi::Place &dst_place,
                     phi::DenseTensor *out) {
  VLOG(3) << "DeviceTransform in, src_place " << in.place()
          << " dst_place: " << dst_place;

  PADDLE_ENFORCE_NE(
      in.place().GetType(),
      dst_place.GetType(),
      common::errors::Unavailable("Currently, model parallelism is only "
                                  "supported between CPU and CUDA."));

  // NOTE(zhiqiu): Special case for CPU->NPU, avoid stream sync.
  if (phi::is_cpu_place(in.place())) {
    paddle::framework::TensorCopy(
        in, dst_place, *phi::DeviceContextPool::Instance().Get(dst_place), out);
    return;
  }

  // NOTE(yy): TransDataDevice should wait for computation of input.
  if (!phi::is_cuda_pinned_place(in.place())) {
    phi::DeviceContextPool::Instance().Get(in.place())->Wait();
    phi::DeviceContextPool::Instance().Get(dst_place)->Wait();
  }

  // FIXME(zcd): TransDataDevice is used to transform data from GPU to CPU and
  // the enforced checks have been done in GetDeviceContext, so the
  // `dev_ctx->Wait()` is necessary. But `dev_ctx->Wait()` will make the program
  // slow, especially when the number of elements is little, for example,
  // the elements of learning rate are one and it's CPU side.
  // One solution is to use a CUDA kernel to complete the copy operation when
  // the transforming is from CPU to GPU and the number of elements is little.
  // But the embarrassment is that this solution makes training slower.
  TensorCopySync(in, dst_place, out);
}

}  // namespace paddle::framework
