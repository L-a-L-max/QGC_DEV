/// @file test_mapping_engine.cc
/// @brief Standalone test for DDSMappingEngine and DDSTransformRegistry.
///
/// Compile and run:
///   cd build && cmake .. -DQGC_ENABLE_DDS=ON && make test_dds_mapping
///   ./test_dds_mapping
///
/// Or manually (without full QGC build):
///   g++ -std=c++20 -DQGC_ENABLE_DDS \
///       -I/path/to/qt6/include -I../../src/DDS -I../../src/Comms/DDSLink \
///       test_mapping_engine.cc \
///       ../../src/DDS/DDSMappingEngine.cc \
///       ../../src/DDS/DDSTransformRegistry.cc \
///       ../../src/DDS/DDSDataInjector.cc \
///       $(pkg-config --cflags --libs Qt6Core) \
///       -lm -o test_dds_mapping

#ifdef QGC_ENABLE_DDS

#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "DDSDataInjector.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <cmath>
#include <iostream>

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        gTestsFailed++; \
    } else { \
        std::cout << "PASS: " << msg << "\n"; \
        gTestsPassed++; \
    } \
} while(0)

#define ASSERT_NEAR(actual, expected, tolerance, msg) do { \
    if (std::abs((actual) - (expected)) > (tolerance)) { \
        std::cerr << "FAIL: " << msg << " (expected=" << expected \
                  << " actual=" << actual << ")\n"; \
        gTestsFailed++; \
    } else { \
        std::cout << "PASS: " << msg << "\n"; \
        gTestsPassed++; \
    } \
} while(0)

/// Create a minimal mapping JSON for testing
static QJsonObject createTestMapping()
{
    QJsonObject root;
    root[QStringLiteral("vendor")] = QStringLiteral("test_vendor");
    root[QStringLiteral("version")] = QStringLiteral("1.0.0");

    QJsonArray topics;

    // Topic 1: attitude
    {
        QJsonObject topic;
        topic[QStringLiteral("dds_topic")] = QStringLiteral("/fmu/out/vehicle_attitude");
        topic[QStringLiteral("fact_group")] = QStringLiteral("vehicle");

        QJsonArray fields;
        {
            QJsonObject f;
            f[QStringLiteral("dds_field")] = QStringLiteral("q[0]");
            f[QStringLiteral("fact_name")] = QStringLiteral("roll");
            f[QStringLiteral("transform")] = QStringLiteral("quaternion_to_euler_roll");
            fields.append(f);
        }
        {
            QJsonObject f;
            f[QStringLiteral("dds_field")] = QStringLiteral("q[0]");
            f[QStringLiteral("fact_name")] = QStringLiteral("pitch");
            f[QStringLiteral("transform")] = QStringLiteral("quaternion_to_euler_pitch");
            fields.append(f);
        }
        {
            QJsonObject f;
            f[QStringLiteral("dds_field")] = QStringLiteral("q[0]");
            f[QStringLiteral("fact_name")] = QStringLiteral("heading");
            f[QStringLiteral("transform")] = QStringLiteral("quaternion_to_euler_yaw");
            fields.append(f);
        }
        topic[QStringLiteral("fields")] = fields;
        topics.append(topic);
    }

    // Topic 2: battery with scale
    {
        QJsonObject topic;
        topic[QStringLiteral("dds_topic")] = QStringLiteral("/fmu/out/battery_status");
        topic[QStringLiteral("fact_group")] = QStringLiteral("battery");

        QJsonArray fields;
        {
            QJsonObject f;
            f[QStringLiteral("dds_field")] = QStringLiteral("voltage_v");
            f[QStringLiteral("fact_name")] = QStringLiteral("voltage");
            fields.append(f);
        }
        {
            QJsonObject f;
            f[QStringLiteral("dds_field")] = QStringLiteral("remaining");
            f[QStringLiteral("fact_name")] = QStringLiteral("percentRemaining");
            f[QStringLiteral("scale")] = 100.0;
            fields.append(f);
        }
        topic[QStringLiteral("fields")] = fields;
        topics.append(topic);
    }

    root[QStringLiteral("topics")] = topics;
    return root;
}

static void testMappingEngine()
{
    std::cout << "\n=== MappingEngine Tests ===\n";

    DDSMappingEngine engine;

    // Load from JSON
    QJsonObject mapping = createTestMapping();
    bool loaded = engine.loadMappingFromJson(mapping);
    ASSERT_TRUE(loaded, "Load mapping from JSON");
    ASSERT_TRUE(engine.vendorName() == "test_vendor", "Vendor name correct");
    ASSERT_TRUE(engine.topicCount() == 2, "Topic count = 2");

    // Look up existing topic
    const DDSTopicMapping *attitude = engine.topicMapping(QStringLiteral("/fmu/out/vehicle_attitude"));
    ASSERT_TRUE(attitude != nullptr, "Attitude topic found");
    ASSERT_TRUE(attitude->fields.size() == 3, "Attitude has 3 field mappings");
    ASSERT_TRUE(attitude->fields[0].factName == "roll", "First field maps to roll");
    ASSERT_TRUE(attitude->fields[0].transform == "quaternion_to_euler_roll", "Roll uses quaternion transform");

    // Look up battery topic
    const DDSTopicMapping *battery = engine.topicMapping(QStringLiteral("/fmu/out/battery_status"));
    ASSERT_TRUE(battery != nullptr, "Battery topic found");
    ASSERT_TRUE(battery->fields[1].scale == 100.0, "Battery remaining scale = 100");

    // Look up non-existing topic
    const DDSTopicMapping *missing = engine.topicMapping(QStringLiteral("/fmu/out/nonexistent"));
    ASSERT_TRUE(missing == nullptr, "Non-existing topic returns nullptr");

    // Namespace stripping: /drone1/fmu/out/vehicle_attitude → /fmu/out/vehicle_attitude
    const DDSTopicMapping *nsStripped = engine.topicMapping(QStringLiteral("/drone1/fmu/out/vehicle_attitude"));
    ASSERT_TRUE(nsStripped != nullptr, "Namespace-prefixed topic resolved via stripping");

    // All topic names
    QStringList allTopics = engine.allTopicNames();
    ASSERT_TRUE(allTopics.size() == 2, "allTopicNames returns 2");
}

static void testTransformRegistry()
{
    std::cout << "\n=== TransformRegistry Tests ===\n";

    DDSTransformRegistry registry;

    ASSERT_TRUE(registry.count() > 0, "Built-in transforms registered");
    ASSERT_TRUE(registry.hasTransform("quaternion_to_euler_roll"), "Has quaternion_to_euler_roll");
    ASSERT_TRUE(registry.hasTransform("rad_to_deg"), "Has rad_to_deg");
    ASSERT_TRUE(registry.hasTransform("negate"), "Has negate");
    ASSERT_TRUE(registry.hasTransform("identity"), "Has identity");

    // Test quaternion → euler: identity quaternion (1,0,0,0) → roll=0
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("q[0]")] = 1.0;  // w
        fields[QStringLiteral("q[1]")] = 0.0;  // x
        fields[QStringLiteral("q[2]")] = 0.0;  // y
        fields[QStringLiteral("q[3]")] = 0.0;  // z

        auto rollFunc = registry.transform("quaternion_to_euler_roll");
        double roll = rollFunc(fields).toDouble();
        ASSERT_NEAR(roll, 0.0, 0.01, "Identity quaternion → roll = 0");

        auto pitchFunc = registry.transform("quaternion_to_euler_pitch");
        double pitch = pitchFunc(fields).toDouble();
        ASSERT_NEAR(pitch, 0.0, 0.01, "Identity quaternion → pitch = 0");

        auto yawFunc = registry.transform("quaternion_to_euler_yaw");
        double yaw = yawFunc(fields).toDouble();
        ASSERT_NEAR(yaw, 0.0, 0.01, "Identity quaternion → yaw = 0");
    }

    // Test quaternion: 45 degree yaw rotation
    {
        const double angle = M_PI / 4.0;  // 45 degrees
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("q[0]")] = std::cos(angle / 2.0);  // w
        fields[QStringLiteral("q[1]")] = 0.0;                     // x
        fields[QStringLiteral("q[2]")] = 0.0;                     // y
        fields[QStringLiteral("q[3]")] = std::sin(angle / 2.0);   // z

        auto yawFunc = registry.transform("quaternion_to_euler_yaw");
        double yaw = yawFunc(fields).toDouble();
        ASSERT_NEAR(yaw, 45.0, 0.1, "45-degree yaw quaternion → heading ≈ 45");
    }

    // Test rad_to_deg
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("_value")] = M_PI;
        auto func = registry.transform("rad_to_deg");
        double result = func(fields).toDouble();
        ASSERT_NEAR(result, 180.0, 0.01, "π radians → 180 degrees");
    }

    // Test negate
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("_value")] = 5.5;
        auto func = registry.transform("negate");
        double result = func(fields).toDouble();
        ASSERT_NEAR(result, -5.5, 0.01, "negate(5.5) = -5.5");
    }

    // Test ground_speed_from_vxy
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("vx")] = 3.0;
        fields[QStringLiteral("vy")] = 4.0;
        auto func = registry.transform("ground_speed_from_vxy");
        double result = func(fields).toDouble();
        ASSERT_NEAR(result, 5.0, 0.01, "ground_speed(3,4) = 5");
    }
}

static void testDataInjector()
{
    std::cout << "\n=== DataInjector Tests ===\n";

    DDSMappingEngine engine;
    DDSTransformRegistry transforms;
    DDSDataInjector injector(&engine, &transforms);

    // Load mapping
    engine.loadMappingFromJson(createTestMapping());

    // Track injected values
    QHash<QString, QVariant> injectedValues;
    QObject::connect(&injector, &DDSDataInjector::factUpdated,
                     [&](const QString &group, const QString &name, const QVariant &value) {
                         injectedValues.insert(group + "/" + name, value);
                     });

    // Simulate attitude message (identity quaternion)
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("q[0]")] = 1.0;
        fields[QStringLiteral("q[1]")] = 0.0;
        fields[QStringLiteral("q[2]")] = 0.0;
        fields[QStringLiteral("q[3]")] = 0.0;

        injector.onDDSMessage(QStringLiteral("/fmu/out/vehicle_attitude"), fields, 0);
    }

    ASSERT_TRUE(injector.messagesProcessed() == 1, "1 message processed");
    ASSERT_TRUE(injector.factsUpdated() == 3, "3 facts updated (roll, pitch, heading)");
    ASSERT_NEAR(injectedValues.value("vehicle/roll").toDouble(), 0.0, 0.01,
                "Injected roll ≈ 0");
    ASSERT_NEAR(injectedValues.value("vehicle/pitch").toDouble(), 0.0, 0.01,
                "Injected pitch ≈ 0");

    // Simulate battery message with scale
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("voltage_v")] = 22.4;
        fields[QStringLiteral("remaining")] = 0.75;

        injector.onDDSMessage(QStringLiteral("/fmu/out/battery_status"), fields, 0);
    }

    ASSERT_TRUE(injector.messagesProcessed() == 2, "2 messages processed");
    ASSERT_NEAR(injectedValues.value("battery/voltage").toDouble(), 22.4, 0.01,
                "Battery voltage = 22.4V");
    ASSERT_NEAR(injectedValues.value("battery/percentRemaining").toDouble(), 75.0, 0.01,
                "Battery remaining = 75% (0.75 × 100)");

    // Simulate unmapped topic
    {
        QHash<QString, QVariant> fields;
        fields[QStringLiteral("something")] = 42;
        injector.onDDSMessage(QStringLiteral("/unknown/topic"), fields, 0);
    }

    ASSERT_TRUE(injector.unmappedSkipped() == 1, "1 unmapped topic skipped");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== QGC DDS Mapping Engine Test Suite ===\n";

    testMappingEngine();
    testTransformRegistry();
    testDataInjector();

    std::cout << "\n=== Results ===\n"
              << "Passed: " << gTestsPassed << "\n"
              << "Failed: " << gTestsFailed << "\n";

    return gTestsFailed > 0 ? 1 : 0;
}

#else
#include <iostream>
int main() {
    std::cout << "DDS support not enabled. Compile with -DQGC_ENABLE_DDS\n";
    return 0;
}
#endif
