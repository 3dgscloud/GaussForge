enable_testing()

if(NOT EMSCRIPTEN)
    include(FetchContent)
    
    # Google Benchmark
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
    )
    
    # Set benchmark options
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(benchmark)
    
    # PLY Reader Benchmark
    add_executable(ply_reader_benchmark ply_reader_benchmark.cpp)
    target_link_libraries(ply_reader_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)
    
    # PLY Writer Benchmark
    add_executable(ply_writer_benchmark ply_writer_benchmark.cpp)
    target_link_libraries(ply_writer_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # PLY Compressed Reader Benchmark
    add_executable(ply_compressed_reader_benchmark ply_compressed_reader_benchmark.cpp)
    target_link_libraries(ply_compressed_reader_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # PLY Compressed Writer Benchmark
    add_executable(ply_compressed_writer_benchmark ply_compressed_writer_benchmark.cpp)
    target_link_libraries(ply_compressed_writer_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # Splat Reader Benchmark
    add_executable(splat_reader_benchmark splat_reader_benchmark.cpp)
    target_link_libraries(splat_reader_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # Splat Writer Benchmark
    add_executable(splat_writer_benchmark splat_writer_benchmark.cpp)
    target_link_libraries(splat_writer_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # Ksplat Reader Benchmark
    add_executable(ksplat_reader_benchmark ksplat_reader_benchmark.cpp)
    target_link_libraries(ksplat_reader_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)

    # Ksplat Writer Benchmark
    add_executable(ksplat_writer_benchmark ksplat_writer_benchmark.cpp)
    target_link_libraries(ksplat_writer_benchmark PRIVATE gauss_forge benchmark::benchmark benchmark::benchmark_main)
endif()
