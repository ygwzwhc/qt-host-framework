cmake_minimum_required(VERSION 3.16)

project(QtHostFramework VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 下源码按 UTF-8 编译，避免中文注释/字符串乱码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt 元对象(MOC) / 资源(RCC) / 界面(QUIC) 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 优先使用 Qt6，找不到则自动回退到 Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Network)

set(PROJECT_TARGET qt_host_framework)

add_executable(${PROJECT_TARGET} WIN32
    src/main.cpp

    # 核心层：通用
    src/core/ByteUtils.h
    src/core/AppLogger.h
    src/core/AppLogger.cpp
    src/core/SerialConfig.h
    src/core/MqttSettings.h
    src/core/ITransport.h
    src/core/FrameCodec.h
    src/core/FrameCodec.cpp

    # 传输层实现
    src/core/transports/SerialTransport.h
    src/core/transports/SerialTransport.cpp
    src/core/transports/TcpClientTransport.h
    src/core/transports/TcpClientTransport.cpp
    src/core/transports/TcpServerTransport.h
    src/core/transports/TcpServerTransport.cpp
    src/core/transports/UdpTransport.h
    src/core/transports/UdpTransport.cpp
    src/core/transports/LoopbackTransport.h
    src/core/transports/LoopbackTransport.cpp
    src/core/transports/MqttTransport.h
    src/core/transports/MqttTransport.cpp

    # 协议层实现
    src/core/protocols/ProtocolTypes.h
    src/core/protocols/ModbusProtocol.h
    src/core/protocols/ModbusProtocol.cpp
    src/core/protocols/AsciiLineCodec.h
    src/core/protocols/AsciiLineCodec.cpp

    # UI 层
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ControlPanel.h
    src/ui/ControlPanel.cpp
    src/ui/LogPanel.h
    src/ui/LogPanel.cpp

    # ViewModel 层（MVVM）
    src/vm/HostViewModel.h
    src/vm/HostViewModel.cpp
    src/vm/PacketLogModel.h
    src/vm/PacketLogModel.cpp
    src/vm/PacketFilterProxy.h
    src/vm/PacketFilterProxy.cpp
)

target_include_directories(${PROJECT_TARGET} PRIVATE src)

target_link_libraries(${PROJECT_TARGET} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Network
)

# 统一输出到 构建目录/bin/，便于拷贝运行
set_target_properties(${PROJECT_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
