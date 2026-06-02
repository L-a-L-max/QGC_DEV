
#ifndef PX4_DDS_VEHICLESTATUS_H
#define PX4_DDS_VEHICLESTATUS_H

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px4_msgs_msg_dds__VehicleStatus_
{
  uint64_t timestamp;
  uint8_t armed_time;
  uint8_t takeoff_time;
  uint8_t arming_state;
  uint8_t latest_arming_reason;
  uint8_t latest_disarming_reason;
  uint64_t nav_state_timestamp;
  uint8_t nav_state;
  uint8_t nav_state_user_intention;
  uint8_t failure_detector_status;
  uint8_t hil_state;
  uint8_t vehicle_type;
  bool failsafe;
  uint8_t failsafe_and_target_state;
  bool is_vtol;
  bool is_vtol_tailsitter;
  bool in_transition_mode;
  bool in_transition_to_fw;
  bool gcs_connection_lost;
  uint8_t gcs_connection_lost_counter;
  bool high_latency_data_link_lost;
  bool power_input_valid;
  bool usb_connected;
  bool open_drone_id_system_present;
  bool open_drone_id_system_healthy;
  bool parachute_system_present;
  bool parachute_system_healthy;
  bool avoidance_system_required;
  bool avoidance_system_valid;
  uint8_t rc_calibrated;
} px4_msgs_msg_dds__VehicleStatus_;

extern const dds_topic_descriptor_t px4_msgs_msg_dds__VehicleStatus__desc;

#define px4_msgs_msg_dds__VehicleStatus___alloc() \
((px4_msgs_msg_dds__VehicleStatus_*) dds_alloc (sizeof (px4_msgs_msg_dds__VehicleStatus_)));

#define px4_msgs_msg_dds__VehicleStatus__free(d,o) \
dds_sample_free ((d), &px4_msgs_msg_dds__VehicleStatus__desc, (o))

#ifdef __cplusplus
}
#endif

#endif /* PX4_DDS_VEHICLESTATUS_H */
