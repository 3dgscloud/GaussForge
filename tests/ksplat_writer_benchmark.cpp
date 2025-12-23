#include "gf/core/gauss_ir.h"
#include "gf/io/ksplat.h"

#include <benchmark/benchmark.h>
#include <cmath>
#include <random>
#include <vector>

namespace {

// Create realistic ksplat test data (simulating real Gaussian Splatting data)
// .ksplat format supports SH degrees 0-3, we test with SH degree 3
gf::GaussianCloudIR CreateRealisticKsplatData(int numPoints) {
  gf::GaussianCloudIR ir;
  ir.numPoints = numPoints;
  ir.meta.shDegree = 3; // Real ksplat files typically use SH degree 3
  ir.meta.sourceFormat = "ksplat";

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
    float norm = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
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

    // Alpha: pre-sigmoid opacity
    ir.alphas.push_back(alphaDist(gen));

    // Color: DC coefficients (RGB)
    ir.colors.push_back(colorDist(gen));
    ir.colors.push_back(colorDist(gen));
    ir.colors.push_back(colorDist(gen));

    // SH coefficients: 45 coefficients (SH degree 3)
    for (int j = 0; j < shDim; ++j) {
      ir.sh.push_back(shDist(gen)); // R
      ir.sh.push_back(shDist(gen)); // G
      ir.sh.push_back(shDist(gen)); // B
    }
  }

  return ir;
}

// Ksplat write speed benchmark
static void BM_KsplatWriter_Write(benchmark::State &state) {
  const int numPoints = static_cast<int>(state.range(0));

  // Prepare test data (outside loop to avoid affecting benchmark)
  auto writer = gf::MakeKsplatWriter();
  auto testIR = CreateRealisticKsplatData(numPoints);

  // Run benchmark
  for (auto _ : state) {
    auto result = writer->Write(testIR, {});
    if (!result.ok()) {
      state.SkipWithError("Write failed");
      return;
    }
    benchmark::DoNotOptimize(result.value());

    // Set counters inside loop (per iteration)
    const size_t dataSize = result.value().size();
    state.SetBytesProcessed(static_cast<int64_t>(dataSize));
    state.SetItemsProcessed(static_cast<int64_t>(numPoints));
  }
}

// Register benchmark: Test write speed for different ksplat file sizes
// Using common point count ranges: 1K, 10K, 100K, 500K, 1M, 5M
BENCHMARK(BM_KsplatWriter_Write)
    ->Arg(1000)    // 1K points
    ->Arg(10000)   // 10K points
    ->Arg(100000)  // 100K points
    ->Arg(500000)  // 500K points
    ->Arg(1000000) // 1M points
    ->Arg(5000000) // 5M points
    ->MinTime(2.0)
    ->ReportAggregatesOnly(true)
    ->Unit(benchmark::kMillisecond);

} // namespace

BENCHMARK_MAIN();

