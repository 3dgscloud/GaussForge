#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"

namespace gf {

struct WriteOptions {
  bool strict = false;
  // SPZ output version. Ignored by non-SPZ writers.
  // Keep v3 as the default for compatibility with readers that do not yet
  // support SPZ v4.
  uint32_t spzVersion = 3;
};

class IGaussWriter {
public:
  virtual ~IGaussWriter() = default;
  // Write data to memory buffer, return serialized byte data
  virtual Expected<std::vector<uint8_t>> Write(const GaussianCloudIR &ir,
                                               const WriteOptions &options) = 0;
};

} // namespace gf
