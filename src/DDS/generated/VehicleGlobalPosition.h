
#ifndef PX4_DDS_VEHICLEGLOBALPOSITION_H
#define PX4_DDS_VEHICLEGLOBALPOSITION_H

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px4_msgs_msg_dds__VehicleGlobalPosition_
{
  uint64_t timestamp;
  uint64_t timestamp_sample;
  double lat;
  double lon;
  float alt;
  float alt_ellipsoid;
  bool lat_lon_valid;
  bool alt_valid;
  float delta_alt;
  float delta_terrain;
  uint8_t lat_lon_reset_counter;
  uint8_t alt_reset_counter;
  uint8_t terrain_reset_counter;
  float eph;
  float epv;
  float terrain_alt;
  bool terrain_alt_valid;
  bool dead_reckoning;
} px4_msgs_msg_dds__VehicleGlobalPosition_;

extern const dds_topic_descriptor_t px4_msgs_msg_dds__VehicleGlobalPosition__desc;

#define px4_msgs_msg_dds__VehicleGlobalPosition___alloc() \
((px4_msgs_msg_dds__VehicleGlobalPosition_*) dds_alloc (sizeof (px4_msgs_msg_dds__VehicleGlobalPosition_)));

#define px4_msgs_msg_dds__VehicleGlobalPosition__free(d,o) \
dds_sample_free ((d), &px4_msgs_msg_dds__VehicleGlobalPosition__desc, (o))

#ifdef __cplusplus
}
#endif

#endif /* PX4_DDS_VEHICLEGLOBALPOSITION_H */
