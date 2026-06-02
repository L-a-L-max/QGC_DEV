
#ifndef PX4_DDS_BATTERYSTATUS_H
#define PX4_DDS_BATTERYSTATUS_H

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px4_msgs_msg_dds__BatteryStatus_
{
  uint64_t timestamp;
  bool connected;
  float voltage_v;
  float current_a;
  float current_average_a;
  float discharged_mah;
  float remaining;
  float scale;
  float time_remaining_s;
  float temperature;
  uint8_t cell_count;
  uint8_t source;
  uint8_t priority;
  uint16_t capacity;
  uint16_t cycle_count;
  uint16_t average_time_to_empty;
  uint16_t manufacture_date;
  uint16_t state_of_health;
  uint16_t max_error;
  uint8_t id;
  uint16_t interface_error;
  float voltage_cell_v[14];
  float max_cell_voltage_delta;
  bool is_powering_off;
  bool is_required;
  uint8_t warning;
  uint8_t mode;
  float faults;
  uint32_t custom_faults;
} px4_msgs_msg_dds__BatteryStatus_;

extern const dds_topic_descriptor_t px4_msgs_msg_dds__BatteryStatus__desc;

#define px4_msgs_msg_dds__BatteryStatus___alloc() \
((px4_msgs_msg_dds__BatteryStatus_*) dds_alloc (sizeof (px4_msgs_msg_dds__BatteryStatus_)));

#define px4_msgs_msg_dds__BatteryStatus__free(d,o) \
dds_sample_free ((d), &px4_msgs_msg_dds__BatteryStatus__desc, (o))

#ifdef __cplusplus
}
#endif

#endif /* PX4_DDS_BATTERYSTATUS_H */
