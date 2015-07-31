#ifndef _AHRS_H_
#define _AHRS_H_


struct sensor_axis_t
{
    double x;
    double y;
    double z;
};


void orientation_show(struct sensor_axis_t *accel,
                      struct sensor_axis_t *gyro,
                      struct sensor_axis_t *magn,
                      double magnetic_declination_mrad,
                      int pressure,
                      double temperature);


#endif // _AHRS_H_
