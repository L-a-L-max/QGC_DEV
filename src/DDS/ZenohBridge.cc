#ifdef QGC_ENABLE_DDS

#include "ZenohBridge.h"

#include <QtCore/QDebug>

#ifdef Q_OS_ANDROID
#include <QtCore/QJniObject>
#include <QtCore/QCoreApplication>
#endif

ZenohBridge::ZenohBridge(QObject *parent)
    : QObject(parent)
{
}

ZenohBridge::~ZenohBridge()
{
    stop();
}

bool ZenohBridge::start(const QString &endpoint, int domainId)
{
#ifdef Q_OS_ANDROID
    QJniObject jEndpoint = QJniObject::fromString(endpoint);
    QJniObject context = QJniObject(QNativeInterface::QAndroidApplication::context());

    jboolean ok = QJniObject::callStaticMethod<jboolean>(
        "org/mavlink/qgroundcontrol/ZenohBridgeManager",
        "start",
        "(Landroid/content/Context;Ljava/lang/String;I)Z",
        context.object<jobject>(),
        jEndpoint.object<jstring>(),
        static_cast<jint>(domainId));

    if (ok) {
        qInfo() << "[ZenohBridge] Started, endpoint:" << endpoint << "domain:" << domainId;
        emit started();
    } else {
        const QString err = lastError();
        qWarning() << "[ZenohBridge] Failed to start:" << err;
        emit errorOccurred(err);
    }
    return ok;
#else
    Q_UNUSED(endpoint);
    Q_UNUSED(domainId);
    qInfo() << "[ZenohBridge] Not available on this platform (Android only)";
    return false;
#endif
}

void ZenohBridge::stop()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(
        "org/mavlink/qgroundcontrol/ZenohBridgeManager",
        "stop",
        "()V");
    qInfo() << "[ZenohBridge] Stopped";
    emit stopped();
#endif
}

bool ZenohBridge::isRunning() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(
        "org/mavlink/qgroundcontrol/ZenohBridgeManager",
        "isRunning",
        "()Z");
#else
    return false;
#endif
}

QString ZenohBridge::statusText() const
{
#ifdef Q_OS_ANDROID
    QJniObject jStatus = QJniObject::callStaticObjectMethod(
        "org/mavlink/qgroundcontrol/ZenohBridgeManager",
        "getStatusText",
        "()Ljava/lang/String;");
    return jStatus.isValid() ? jStatus.toString() : QStringLiteral("unknown");
#else
    return QStringLiteral("not available (non-Android)");
#endif
}

QString ZenohBridge::lastError() const
{
#ifdef Q_OS_ANDROID
    QJniObject jErr = QJniObject::callStaticObjectMethod(
        "org/mavlink/qgroundcontrol/ZenohBridgeManager",
        "getLastError",
        "()Ljava/lang/String;");
    return jErr.isValid() ? jErr.toString() : QString();
#else
    return QString();
#endif
}

#endif // QGC_ENABLE_DDS
