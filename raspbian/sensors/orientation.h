#ifndef _ORIENTATION_H_
#define _ORIENTATION_H_


struct sensor_axis_t
{
    double x;
    double y;
    double z;
};


void orientation_init(struct sensor_axis_t *accel, struct sensor_axis_t *magn);
void orientation_show(struct sensor_axis_t *accel,
                      struct sensor_axis_t *gyro,
                      struct sensor_axis_t *magn);


#endif // _ORIENTATION_H_
