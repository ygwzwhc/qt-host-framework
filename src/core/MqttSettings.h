#pragma once

#include <QString>
#include <QtGlobal>

/**
 * MqttSettings —— MQTT 通道连接参数。
 */
struct MqttSettings
{
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 1883;
    QString clientId;                        // 空则自动生成 QtHost_xxxx
    QString username;                        // 可选
    QString password;                        // 可选
    QString pubTopic = QStringLiteral("qt-host/tx");
    QString subTopic = QStringLiteral("qt-host/rx");
    int keepAliveSec = 60;
};
