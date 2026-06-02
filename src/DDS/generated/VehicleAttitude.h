
#ifndef PX4_DDS_VEHICLEATTITUDE_H
#define PX4_DDS_VEHICLEATTITUDE_H

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px4_msgs_msg_dds__VehicleAttitude_
{
  uint64_t timestamp;
  uint64_t timestamp_sample;
  float q[4];
  float delta_q_reset[4];
  uint8_t quat_reset_counter;
} px4_msgs_msg_dds__VehicleAttitude_;

extern const dds_topic_descriptor_t px4_msgs_msg_dds__VehicleAttitude__desc;

#define px4_msgs_msg_dds__VehicleAttitude___alloc() \
((px4_msgs_msg_dds__VehicleAttitude_*) dds_alloc (sizeof (px4_msgs_msg_dds__VehicleAttitude_)));

#define px4_msgs_msg_dds__VehicleAttitude__free(d,o) \
dds_sample_free ((d), &px4_msgs_msg_dds__VehicleAttitude__desc, (o))

#ifdef __cplusplus
}
#endif

#endif /* PX4_DDS_VEHICLEATTITUDE_H */
