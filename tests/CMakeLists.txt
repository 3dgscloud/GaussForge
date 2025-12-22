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
endif()
