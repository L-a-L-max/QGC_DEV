#ifdef QGC_ENABLE_DDS

#include "DDSMappingEngine.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>

DDSMappingEngine::DDSMappingEngine(QObject *parent)
    : QObject(parent)
{
}

DDSMappingEngine::~DDSMappingEngine() = default;

bool DDSMappingEngine::loadMapping(const QString &mappingName)
{
    const QString path = _resolveFilePath(mappingName);
    if (path.isEmpty()) {
        qWarning() << "[MappingEngine] Mapping file not found:" << mappingName;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[MappingEngine] Cannot open:" << path;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[MappingEngine] JSON parse error in" << path
                    << ":" << parseError.errorString();
        return false;
    }

    return loadMappingFromJson(doc.object());
}

bool DDSMappingEngine::loadMappingFromJson(const QJsonObject &root)
{
    _vendorName = root.value(QStringLiteral("vendor")).toString();
    _version = root.value(QStringLiteral("version")).toString();

    const QJsonArray topics = root.value(QStringLiteral("topics")).toArray();
    if (topics.isEmpty()) {
        qWarning() << "[MappingEngine] No topics in mapping";
        return false;
    }

    _topicMap.clear();
    for (const QJsonValue &val : topics) {
        if (!_parseTopic(val.toObject())) {
            return false;
        }
    }

    qInfo() << "[MappingEngine] Loaded" << _vendorName << "v" << _version
            << ":" << _topicMap.size() << "topics," << fieldCount() << "fields";
    return true;
}

bool DDSMappingEngine::loadVendorOverlay(const QString &vendorName)
{
    const QString path = _resolveFilePath(vendorName);
    if (path.isEmpty()) {
        qWarning() << "[MappingEngine] Vendor overlay not found:" << vendorName;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray topics = root.value(QStringLiteral("topics")).toArray();
    for (const QJsonValue &val : topics) {
        const QJsonObject topicObj = val.toObject();
        const QString topicName = topicObj.value(QStringLiteral("dds_topic")).toString();

        // Overlay: replace entire topic if it exists, otherwise add new
        _topicMap.remove(topicName);
        _parseTopic(topicObj);
    }

    qInfo() << "[MappingEngine] Applied vendor overlay:" << vendorName
            << "(" << topics.size() << "topics)";
    return true;
}

const DDSTopicMapping *DDSMappingEngine::topicMapping(const QString &ddsTopicName) const
{
    auto it = _topicMap.constFind(ddsTopicName);
    if (it != _topicMap.constEnd()) {
        return &it.value();
    }

    // Try without namespace prefix (strip everything before the last /fmu/)
    QString candidate = ddsTopicName;
    const int fmuIdx = candidate.lastIndexOf(QStringLiteral("/fmu/"));
    if (fmuIdx > 0) {
        candidate = candidate.mid(fmuIdx);
        auto it2 = _topicMap.constFind(candidate);
        if (it2 != _topicMap.constEnd()) {
            return &it2.value();
        }
    }

    // Try stripping PX4 version suffix (_v1, _v2, etc.)
    static const QRegularExpression versionSuffix(QStringLiteral("_v\\d+$"));
    const QString stripped = candidate.contains(versionSuffix)
                                 ? candidate.left(candidate.lastIndexOf(QStringLiteral("_v")))
                                 : QString();
    if (!stripped.isEmpty()) {
        auto it3 = _topicMap.constFind(stripped);
        if (it3 != _topicMap.constEnd()) {
            return &it3.value();
        }
    }

    return nullptr;
}

QStringList DDSMappingEngine::allTopicNames() const
{
    return _topicMap.keys();
}

int DDSMappingEngine::fieldCount() const
{
    int count = 0;
    for (auto it = _topicMap.constBegin(); it != _topicMap.constEnd(); ++it) {
        count += it.value().fields.size();
    }
    return count;
}

bool DDSMappingEngine::_parseTopic(const QJsonObject &topicObj)
{
    DDSTopicMapping mapping;
    mapping.ddsTopicName = topicObj.value(QStringLiteral("dds_topic")).toString();
    mapping.ddsTypeName = topicObj.value(QStringLiteral("dds_type")).toString();
    mapping.defaultFactGroup = topicObj.value(QStringLiteral("fact_group")).toString();

    if (mapping.ddsTopicName.isEmpty()) {
        qWarning() << "[MappingEngine] Topic missing dds_topic field";
        return false;
    }

    const QJsonArray fields = topicObj.value(QStringLiteral("fields")).toArray();
    mapping.fields.reserve(fields.size());
    for (const QJsonValue &fieldVal : fields) {
        mapping.fields.append(_parseField(fieldVal.toObject(), mapping.defaultFactGroup));
    }

    _topicMap.insert(mapping.ddsTopicName, mapping);
    return true;
}

DDSFieldMapping DDSMappingEngine::_parseField(const QJsonObject &fieldObj,
                                               const QString &defaultGroup) const
{
    DDSFieldMapping field;
    field.ddsField  = fieldObj.value(QStringLiteral("dds_field")).toString();
    field.factName  = fieldObj.value(QStringLiteral("fact_name")).toString();
    field.factGroup = fieldObj.value(QStringLiteral("fact_group")).toString(defaultGroup);
    field.transform = fieldObj.value(QStringLiteral("transform")).toString();
    field.scale     = fieldObj.value(QStringLiteral("scale")).toDouble(1.0);
    field.offset    = fieldObj.value(QStringLiteral("offset")).toDouble(0.0);
    return field;
}

QString DDSMappingEngine::_resolveFilePath(const QString &mappingName) const
{
    // Search order:
    // 1. User's config directory: ~/.config/QGroundControl/dds_mappings/
    // 2. Application resource: :/dds_mappings/
    // 3. Relative to executable: ./resources/dds_mappings/

    const QString fileName = mappingName + QStringLiteral(".json");

    // User config directory
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString userPath = configDir + QStringLiteral("/dds_mappings/") + fileName;
    if (QFile::exists(userPath)) {
        return userPath;
    }

    // Qt resource
    const QString resourcePath = QStringLiteral(":/dds_mappings/") + fileName;
    if (QFile::exists(resourcePath)) {
        return resourcePath;
    }

    // Relative to app
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString relPath = appDir + QStringLiteral("/resources/dds_mappings/") + fileName;
    if (QFile::exists(relPath)) {
        return relPath;
    }

    return {};
}

#endif // QGC_ENABLE_DDS
