#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * ITransport —— 统一传输层抽象（框架核心接口）。
 *
 * 所有"能收发字节"的通道都实现本接口：
 *   串口 / TCP 客户端 / TCP 服务器 / UDP / 回环(自测)
 * UI 与协议层只依赖本接口，不关心底层实现。
 *
 * 数据约定：
 *   - 打开成功/失败后分别发 opened / errorHappened；
 *   - 关闭统一发 closed；
 *   - 收到的原始字节发 bytesReceived；写入的字节发 bytesSent（供监视）。
 */
class ITransport : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~ITransport() override = default;

    // 尝试打开（TCP 等异步通道返回 true 仅表示已开始连接，以信号为准）
    virtual bool open(QString *errMsg = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual QString name() const = 0;          // 通道名，如 "串口"
    virtual QString detail() const = 0;        // 当前参数描述，用于状态栏

    virtual void write(const QByteArray &bytes) = 0;

signals:
    void opened();
    void closed();
    void errorHappened(const QString &message);
    void bytesReceived(const QByteArray &bytes);
    void bytesSent(const QByteArray &bytes);
};
