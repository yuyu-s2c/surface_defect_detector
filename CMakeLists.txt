cmake_minimum_required(VERSION 3.16)

project(surface_defect_detector VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# OpenCV-MinGW-Build 预编译包，OpenCVConfig.cmake 位于 x64/mingw/lib
find_package(OpenCV REQUIRED PATHS "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/lib")

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
    src/DetectionController.h
    src/DetectionController.cpp
    src/ResultEvaluator.h
    src/ResultEvaluator.cpp
)

qt_add_executable(surface_defect_detector ${PROJECT_SOURCES})

target_include_directories(surface_defect_detector PRIVATE src)

target_link_libraries(surface_defect_detector PRIVATE
    Qt6::Widgets
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
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
endif()
