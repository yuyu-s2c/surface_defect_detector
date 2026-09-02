cmake_minimum_required(VERSION 3.16)

project(surface_defect_detector VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# OpenCV-MinGW-Build 预编译包，OpenCVConfig.cmake 位于 x64/mingw/lib
find_package(OpenCV REQUIRED PATHS "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/lib")

# ONNX Runtime（Phase 2 DL 推理）：官方 MSVC 构建，C ABI 与 MinGW 兼容。
# GNU ld 直接以 onnxruntime.dll 为链接输入自动生成导入项（已用 tools/ort_demo 验证），
# 不使用 MSVC 的 .lib 导入库。
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/DatasetManager.h
    src/DatasetManager.cpp
    src/ImageViewWidget.h
    src/ImageViewWidget.cpp
    src/IDetectionEngine.h
    src/DetectionEngine.h
    src/DetectionEngine.cpp
    src/DLDetectionEngine.h
    src/DLDetectionEngine.cpp
    src/DetectionController.h
    src/DetectionController.cpp
    src/EngineParams.h
    src/ResultExporter.h
    src/ResultExporter.cpp
    src/ResultEvaluator.h
    src/ResultEvaluator.cpp
)

qt_add_executable(surface_defect_detector ${PROJECT_SOURCES})

target_include_directories(surface_defect_detector PRIVATE
    src
    "${ONNXRUNTIME_DIR}/include"
)

target_link_libraries(surface_defect_detector PRIVATE
    Qt6::Widgets
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
)

if(WIN32)
    set_target_properties(surface_defect_detector PROPERTIES WIN32_EXECUTABLE OFF)

    # 构建后把 OpenCV dll 复制到可执行文件旁，保证直接运行不报错
    add_custom_command(TARGET surface_defect_detector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_core455.dll"
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_imgproc455.dll"
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_imgcodecs455.dll"
            $<TARGET_FILE_DIR:surface_defect_detector>
    )
    # ONNX Runtime dll（providers_shared 是 onnxruntime.dll 的运行期依赖）
    add_custom_command(TARGET surface_defect_detector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
            "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:surface_defect_detector>
    )
endif()
