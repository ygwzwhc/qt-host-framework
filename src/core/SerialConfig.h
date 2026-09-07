#pragma once

#include <QString>
#include <QtSerialPort/QSerialPort>

/**
 * 串口参数聚合结构：把"参数"与"打开串口的动作"解耦。
 * UI 面板负责编辑它，SerialPortManager 负责消费它。
 */
struct SerialConfig
{
    QString portName;
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};
