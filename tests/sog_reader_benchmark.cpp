#include "gf/core/gauss_ir.h"
#include "gf/io/sog.h"

#include <benchmark/benchmark.h>
#include <cmath>
#include <random>
#include <vector>

namespace {

// Create realistic SOG test data (simulating real Gaussian Splatting data)
// SOG format supports SH degrees 0-3, we test with SH degree 3
gf::GaussianCloudIR CreateRealisticSOGData(int numPoints) {
  gf::GaussianCloudIR ir;
  ir.numPoints = numPoints;
  ir.meta.shDegree = 3; // Real SOG files typically use SH degree 3
  ir.meta.sourceFormat = "sog";

  const int shCoeffsPerPoint = gf::ShCoeffsPerPoint(3);
  const int shDim = shCoeffsPerPoint / 3; // 45 coefficients

  ir.positions.reserve(numPoints * 3);
  ir.scales.reserve(numPoints * 3);
  ir.rotations.reserve(numPoints * 4);
  ir.alphas.reserve(numPoints);
  ir.colors.reserve(numPoints * 3);
  ir.sh.reserve(numPoints * shCoeffsPerPoint);

  // Use random number generator to create more realistic data
  std::mt19937 gen(42); // Fixed seed for reproducibility
  std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
  std::uniform_real_distribution<float> scaleDist(-3.0f, 0.0f); // log scale
  std::uniform_real_distribution<float> alphaDist(-5.0f, 5.0f); // pre-sigmoid
  std::uniform_real_distribution<float> colorDist(-1.0f, 1.0f);
  std::uniform_real_distribution<float> shDist(-0.5f, 0.5f);

  for (int i = 0; i < numPoints; ++i) {
    // Position: random position
    ir.positions.push_back(posDist(gen));
    ir.positions.push_back(posDist(gen));
    ir.positions.push_back(posDist(gen));

    // Scale: log scale values
    ir.scales.push_back(scaleDist(gen));
    ir.scales.push_back(scaleDist(gen));
    ir.scales.push_back(scaleDist(gen));

    // Rotation: normalized quaternion
    float qx = (gen() % 2000 - 1000) / 1000.0f;
    float qy = (gen() % 2000 - 1000) / 1000.0f;
    float qz = (gen() % 2000 - 1000) / 1000.0f;
    float qw = (gen() % 2000 - 1000) / 1000.0f;
    
    // Normalize quaternion
    float norm = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
    if (norm > 0.001f) {
      qx /= norm;
      qy /= norm;
      qz /= norm;
      qw /= norm;
    }
    ir.rotations.push_back(qw);
    ir.rotations.push_back(qx);
    ir.rotations.push_back(qy);
    ir.rotations.push_back(qz);

    // Alpha: pre-sigmoid value
    ir.alphas.push_back(alphaDist(gen));

    // Color: normalized RGB values
    ir.colors.push_back(colorDist(gen));
    ir.colors.push_back(colorDist(gen));
    ir.colors.push_back(colorDist(gen));

    // Spherical harmonics: realistic range
    for (int j = 0; j < shCoeffsPerPoint; ++j) {
      ir.sh.push_back(shDist(gen));
    }
  }

  return ir;
}

// Write test data to SOG file and measure performance
void BM_SogReader_Read(benchmark::State& state) {
  const int numPoints = state.range(0);
  
  // Create test data
  auto ir = CreateRealisticSOGData(numPoints);
  
  // Write to memory
  auto writer = gf::MakeSogWriter();
  auto writeResult = writer->Write(ir, {});
  if (!writeResult.ok()) {
    state.SkipWithError("Failed to write test SOG data");
    return;
  }
  const auto& data = writeResult.value();

  // Benchmark reading
  for (auto _ : state) {
    auto reader = gf::MakeSogReader();
    auto readIrResult = reader->Read(data.data(), data.size(), {});
    
    if (!readIrResult.ok()) {
      state.SkipWithError("Read SOG data failed");
      return;
    }
    
    if (readIrResult.value().numPoints != numPoints) {
      state.SkipWithError("Read SOG data mismatch");
      return;
    }
  }

  // Set performance metrics
  state.SetBytesProcessed(static_cast<int64_t>(data.size()));
  state.SetItemsProcessed(static_cast<int64_t>(numPoints));
}

} // namespace

// Register SOG benchmarks with standard test sizes
BENCHMARK(BM_SogReader_Read)
    ->Arg(1000)    // 1K points
    ->Arg(10000)   // 10K points
    ->Arg(100000)  // 100K points
    ->Arg(500000)  // 500K points
    ->Arg(1000000) // 1M points
    ->Arg(5000000) // 5M points
    ->MinTime(2.0)
    ->ReportAggregatesOnly(true)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();