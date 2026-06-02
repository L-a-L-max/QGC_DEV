
#ifndef PX4_DDS_VEHICLELOCALPOSITION_H
#define PX4_DDS_VEHICLELOCALPOSITION_H

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px4_msgs_msg_dds__VehicleLocalPosition_
{
  uint64_t timestamp;
  uint64_t timestamp_sample;
  bool xy_valid;
  bool z_valid;
  bool v_xy_valid;
  bool v_z_valid;
  float x;
  float y;
  float z;
  float vx;
  float vy;
  float vz;
  float z_deriv;
  float ax;
  float ay;
  float az;
  float heading;
  float heading_var;
  float unaided_heading;
  bool heading_good_for_control;
  bool heading_reset;
  float delta_heading;
  float delta_xy[2];
  float delta_z;
  float delta_vxy[2];
  float delta_vz;
  uint8_t xy_reset_counter;
  uint8_t z_reset_counter;
  uint8_t vxy_reset_counter;
  uint8_t vz_reset_counter;
  uint8_t heading_reset_counter;
  bool xy_global;
  bool z_global;
  float ref_lat;
  float ref_lon;
  float ref_alt;
  uint64_t ref_timestamp;
  float dist_bottom;
  bool dist_bottom_valid;
  float dist_bottom_sensor_bitfield;
  float eph;
  float epv;
  float evh;
  float evv;
  float vxy_max;
  float vz_max;
  float hagl_min;
  float hagl_max;
} px4_msgs_msg_dds__VehicleLocalPosition_;

extern const dds_topic_descriptor_t px4_msgs_msg_dds__VehicleLocalPosition__desc;

#define px4_msgs_msg_dds__VehicleLocalPosition___alloc() \
((px4_msgs_msg_dds__VehicleLocalPosition_*) dds_alloc (sizeof (px4_msgs_msg_dds__VehicleLocalPosition_)));

#define px4_msgs_msg_dds__VehicleLocalPosition__free(d,o) \
dds_sample_free ((d), &px4_msgs_msg_dds__VehicleLocalPosition__desc, (o))

#ifdef __cplusplus
}
#endif

#endif /* PX4_DDS_VEHICLELOCALPOSITION_H */
