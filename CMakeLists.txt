cmake_minimum_required(VERSION 3.16)

project(surface_defect_detector VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# MinGW + Ninja 下 AUTOMOC 生成 moc_predefs.h 会因预处理器管道返回 1 失败；关掉预定义宏探测不影响 moc。
set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Qml Gui)

# OpenCV-MinGW-Build 预编译包，OpenCVConfig.cmake 位于 x64/mingw/lib
find_package(OpenCV REQUIRED PATHS "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/lib")

# ONNX Runtime DirectML 包（Phase 3.6）：MSVC 构建的 C ABI，MinGW 可链 dll。
# 缺包时：powershell -File tools/fetch_onnxruntime_dml.ps1
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(PROJECT_SOURCES
    src/main.cpp
    src/DatasetManager.h
    src/DatasetManager.cpp
    src/IDetectionEngine.h
    src/DetectionEngine.h
    src/DetectionEngine.cpp
    src/DLDetectionEngine.h
    src/DLDetectionEngine.cpp
    src/DetectionController.h
    src/DetectionController.cpp
    src/EngineParams.h
    src/OverlayColors.h
    src/ResultExporter.h
    src/ResultExporter.cpp
    src/ResultEvaluator.h
    src/ResultEvaluator.cpp
    src/InspectionSession.h
    src/InspectionSession.cpp
    src/LiveSessionTypes.h
    src/sources/IFrameSource.h
    src/sources/IRejectSink.h
    src/sources/LogRejectSink.h
    src/sources/LogRejectSink.cpp
    src/sources/FileRejectSink.h
    src/sources/FileRejectSink.cpp
    src/sources/CompositeRejectSink.h
    src/sources/CompositeRejectSink.cpp
    src/sources/FolderSource.h
    src/sources/FolderSource.cpp
    src/sources/CameraSource.h
    src/sources/CameraSource.cpp
)

qt_add_executable(surface_defect_detector ${PROJECT_SOURCES})

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(surface_defect_detector
    URI SurfaceDefect
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/panels/AppHeader.qml
        qml/panels/DatasetPanel.qml
        qml/panels/ViewerPanel.qml
        qml/panels/InspectorPanel.qml
        qml/panels/ResultRail.qml
        qml/components/EngineSwitch.qml
        qml/components/ModeSwitch.qml
        qml/components/OverlayChipBar.qml
        qml/components/VerdictBadge.qml
        qml/components/StatTile.qml
        qml/components/ParamForm.qml
        qml/components/AppTable.qml
        qml/components/BoxTable.qml
        qml/components/MetricsTable.qml
        qml/components/CompareView.qml
        qml/components/BusyOverlay.qml
        qml/components/LiveHud.qml
        qml/components/EmptyState.qml
        qml/components/AppStatusBar.qml
        qml/components/AppButton.qml
        qml/components/AppSpinBox.qml
        qml/components/AppDoubleSpin.qml
        qml/components/ToastBanner.qml
        qml/components/StationBanner.qml
        qml/components/EngineStatusChip.qml
    SOURCES
        src/viewmodels/MainViewModel.h
        src/viewmodels/MainViewModel.cpp
        src/viewmodels/DatasetTreeModel.h
        src/viewmodels/DatasetTreeModel.cpp
        src/viewmodels/BoxListModel.h
        src/viewmodels/BoxListModel.cpp
        src/viewmodels/MetricsListModel.h
        src/viewmodels/MetricsListModel.cpp
        src/viewmodels/CompareListModel.h
        src/viewmodels/CompareListModel.cpp
        src/viewmodels/NgListModel.h
        src/viewmodels/NgListModel.cpp
        src/viewmodels/ImageConvert.h
        src/viewmodels/ImageConvert.cpp
        src/items/InspectionCanvas.h
        src/items/InspectionCanvas.cpp
)

target_include_directories(surface_defect_detector PRIVATE
    src
    src/viewmodels
    src/items
    "${ONNXRUNTIME_DIR}/include"
)

target_link_libraries(surface_defect_detector PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Gui
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    dxgi
)

if(WIN32)
    set_target_properties(surface_defect_detector PROPERTIES WIN32_EXECUTABLE OFF)

    add_custom_command(TARGET surface_defect_detector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_core455.dll"
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_imgproc455.dll"
            "${CMAKE_SOURCE_DIR}/third_party/opencv/x64/mingw/bin/libopencv_imgcodecs455.dll"
            $<TARGET_FILE_DIR:surface_defect_detector>
    )
    add_custom_command(TARGET surface_defect_detector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
            "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:surface_defect_detector>
    )
    if(EXISTS "${ONNXRUNTIME_DIR}/lib/DirectML.dll")
        add_custom_command(TARGET surface_defect_detector POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_DIR}/lib/DirectML.dll"
                $<TARGET_FILE_DIR:surface_defect_detector>
        )
    endif()

    # QML 插件：Quick / Controls / Dialogs。路径按本机 Qt 安装。
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "D:/Qt/6.11.2/mingw_64/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET surface_defect_detector POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}/qml"
                --no-translations
                $<TARGET_FILE:surface_defect_detector>
        )
    endif()
endif()
