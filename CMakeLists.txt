cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook)

option(ENABLE_TSAN "Build tests with ThreadSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT APPLE)
    set(CMAKE_CXX_FLAGS "-O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
endif ()
include(FetchContent)

# --- Fetch GLFW ---------------------------------------
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- Fetch GLFW ---------------------------------------
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8-docking
)
FetchContent_MakeAvailable(imgui)
# --- Fetch GLFW ---------------------------------------
# ImPlot (charts)
FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG        v0.16
)
FetchContent_MakeAvailable(implot)

# ══════════════════════════════════════════
# ImGui library (sources + backends)
# ══════════════════════════════════════════

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${implot_SOURCE_DIR}/implot.cpp
        ${implot_SOURCE_DIR}/implot_items.cpp
        include/UI/OrderBookPanel.h
        include/UI/GraphPanel.h
        include/UI/OrderEntryPanel.h
        include/UI/OrdersPanel.h
        include/Networking/Protocol.h
        include/Networking/TcpServer.h
        include/Domain/OrderPod.h
        include/Memory/OrderPool.h
        include/Concurrency/LockFreeQueue.h
        include/Routing/OrderRouter.h
        include/Domain/TradePod.h
        include/Domain/OrderRequest.h
        include/Domain/Time.h
        include/Concurrency/SymbolWorker.h
)

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${implot_SOURCE_DIR}
)

target_link_libraries(imgui_lib PUBLIC glfw)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(imgui_lib PUBLIC OpenGL::GL)



# --- Fetch GoogleTest ---------------------------------------------------------
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# --- Fetch GoogleBenchmark ----------------------------------------------------
FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest googlebenchmark)

# --- Common sources -----------------------------------------------------------
set(COMMON_SOURCES
        src/Storage/MultisetOrderBookStorage.cpp
        src/Engine/CoreEngine.cpp
        src/Domain/order.cpp
        src/Storage/BinaryOrderBookStorage.cpp
        src/UI/TradeTerminal.cpp
        src/Concurrency/OrderQueue.cpp
        src/Concurrency/Worker.cpp
        src/Concurrency/ClientSimulator.cpp
        src/Networking/Protocol.cpp
        src/Networking/TcpServer.cpp
        src/Memory/OrderPool.cpp
        src/Routing/OrderRouter.cpp
        src/Concurrency/SymbolWorker.cpp
)

add_library(trading_lib STATIC ${COMMON_SOURCES}
        src/UI/OrderBookPanel.cpp
        src/UI/GraphPanel.cpp
        src/UI/OrderEntryPanel.cpp
        src/UI/OrdersPanel.cpp
        src/Concurrency/ClientSimulator.cpp)
target_include_directories(trading_lib PUBLIC include)

add_library(trading_lib_cov STATIC ${COMMON_SOURCES})
target_include_directories(trading_lib_cov PUBLIC include)
target_link_libraries(trading_lib_cov PUBLIC imgui_lib)

# --- Main executable ----------------------------------------------------------
add_executable(LimitOrderBook main.cpp)
target_include_directories(LimitOrderBook PRIVATE include)
target_link_libraries(LimitOrderBook PRIVATE trading_lib imgui_lib)
target_link_libraries(trading_lib PUBLIC imgui_lib)

# ---- Test Server ----------
add_executable(tcp_server_test
        src/Networking/TestServer.cpp
        src/Networking/TcpServer.cpp
)
target_include_directories(tcp_server_test PRIVATE include)

# --- Tests --------------------------------------------------------------------
enable_testing()
include(GoogleTest)

add_executable(run_tests
        Tests/Storage/test_multiset_storage.cpp
        Tests/Domain/test_order.cpp
        Tests/Engine/TestMatchingEngine.cpp
        Tests/Storage/test_binaryorderbook.cpp
        Tests/Engine/TestCoreEngine.cpp
        Tests/Concurrency/TestConcurrency.cpp
        Tests/Concurrency/TestWorker.cpp
        Tests/Networking/TestProtocol.cpp
        Tests/Networking/TestTcpServer.cpp
        Tests/TestEndToEnd.cpp
        Tests/Memory/TestOrderPool.cpp
        Tests/Routing/TestOrderRouter.cpp
)
target_link_libraries(run_tests PRIVATE trading_lib_cov gtest_main )

if((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang") AND NOT ENABLE_TSAN)
    target_compile_options(trading_lib_cov PRIVATE --coverage)
    target_link_options(trading_lib_cov PRIVATE --coverage)
    target_compile_options(run_tests PRIVATE --coverage)
    target_link_options(run_tests PRIVATE --coverage)
endif()

if(ENABLE_TSAN)
    target_compile_options(trading_lib_cov PRIVATE -fsanitize=thread -g -O1)
    target_link_options(trading_lib_cov PRIVATE -fsanitize=thread)
    target_compile_options(run_tests PRIVATE -fsanitize=thread -g -O1)
    target_link_options(run_tests PRIVATE -fsanitize=thread)
endif()

gtest_discover_tests(run_tests)


# Legăm DOAR GoogleTest, eliminând dependența de trading_lib_cov care trăgea AppEngine
target_link_libraries(run_tests PRIVATE gtest gtest_main)

# Comentează temporar acoperirea (coverage) sau TSAN pe run_tests ca să eviți alte conflicte
# if((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang") AND NOT ENABLE_TSAN)
#     target_compile_options(run_tests PRIVATE --coverage)
#     target_link_options(run_tests PRIVATE --coverage)
# endif()

gtest_discover_tests(run_tests)


# --- Benchmarks ---------------------------------------------------------------
add_executable(run_benchmarks
        Tests/benchmarks/OrderBookBenchmarkv2.cpp
        Tests/benchmarks/MultithreadedBenchmark.cpp
)
target_link_libraries(run_benchmarks PRIVATE trading_lib benchmark::benchmark_main)
# ------------ Full Test ------------
add_executable(FullTest Tests/FullTest.cpp)
target_include_directories(FullTest PRIVATE include)
target_link_libraries(FullTest PRIVATE trading_lib)

# --- Emscripten ---------------------------------------------------------------
if (EMSCRIPTEN)
    string(APPEND CMAKE_CXX_FLAGS " -s USE_PTHREADS")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -s ASYNCIFY")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -s PROXY_TO_PTHREAD")

    foreach(file "index.html" "run_webassembly.py")
        configure_file("src/${file}" ${file})
    endforeach(file)
endif()