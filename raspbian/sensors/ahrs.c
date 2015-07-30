#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ahrs.h"

// restrict roll to ±90deg to avoid gimbal lock
//#define RESTRICT_PITCH

static inline double to_degrees(double radians)
{
    return radians * (180.0 / M_PI);
}


static inline double to_radians(double degrees)
{
    return degrees * (M_PI / 180.0);
}


static void vector_cross(const struct sensor_axis_t *a,
                         const struct sensor_axis_t *b,
                         struct sensor_axis_t *out)
{
  out->x = (a->y * b->z) - (a->z * b->y);
  out->y = (a->z * b->x) - (a->x * b->z);
  out->z = (a->x * b->y) - (a->y * b->x);
}


static double vector_dot(const struct sensor_axis_t *a, const struct sensor_axis_t *b)
{
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}


static void vector_normalize(struct sensor_axis_t *a)
{
  double mag = sqrt(vector_dot(a, a));
  a->x /= mag;
  a->y /= mag;
  a->z /= mag;
}


static void get_pitch_roll(struct sensor_axis_t *accel, double *roll, double *pitch)
{
    /* roll: Rotation around the longitudinal axis (the plane body, 'X axis'). -90<=roll<=90    */
    /* roll is positive and increasing when moving downward                                     */
    /*                                                                                          */
    /*                                 y                                                        */
    /*             roll = atan(-----------------)                                               */
    /*                          sqrt(x^2 + z^2)                                                 */
    /* where:  x, y, z are returned value from accelerometer sensor                             */

    /* pitch: Rotation around the lateral axis (the wing span, 'Y axis'). -180<=pitch<=180)     */
    /* pitch is positive and increasing when moving upwards                                     */
    /*                                                                                          */
    /*                                 x                                                        */
    /*            pitch = atan(-----------------)                                               */
    /*                          sqrt(y^2 + z^2)                                                 */
    /* where:  x, y, z are returned value from accelerometer sensor                             */
//#ifdef RESTRICT_PITCH
//    *roll = to_degrees(atan2(accel->y, accel->z));
//    *pitch = to_degrees(atan(accel->x / sqrt(accel->y * accel->y + accel->z * accel->z)));
//#else
//    *roll = to_degrees(atan(accel->y / sqrt(accel->x * accel->x + accel->z * accel->z)));
//    *pitch = to_degrees(atan2(accel->x, accel->z));
//#endif
    *roll = to_degrees(atan(accel->y / sqrt(accel->x * accel->x + accel->z * accel->z)));
    *pitch = to_degrees(atan(-accel->x / sqrt(accel->y * accel->y + accel->z * accel->z)));
}


static void get_yaw(struct sensor_axis_t *magn, double roll, double pitch, double *yaw)
{
    /* Sensor rotates around Z-axis                                                           */
    /* yaw is the angle between the 'X axis' and magnetic north on the horizontal plane (Oxy) */
    /* heading = atan(My / Mx)                                                                */
    //double rollAngle = to_radians(kalAngleX);
    //double pitchAngle = to_radians(kalAngleY);
    //double Bfy = -magn->z * sin(rollAngle) - magn->y * cos(rollAngle);
    //double Bfx = -magn->x * cos(pitchAngle) +
    //             magn->y * sin(pitchAngle) * sin(rollAngle) +
    //             (-1)*magn->z * sin(pitchAngle) * cos(rollAngle);
    //*yaw = to_degrees(atan2(Bfy, Bfx));

    //double rollAngle = to_radians(kalAngleX);
    //double pitchAngle = to_radians(kalAngleY);
    //*yaw = to_degrees(atan2((magn->z * sin(rollAngle) - magn->y * cos(rollAngle))*(-1),
    //                        magn->x * cos(pitchAngle) +
    //                        magn->y * sin(pitchAngle) * sin(rollAngle) +
    //                        magn->z * sin(pitchAngle) * cos(rollAngle)));

    *yaw = to_degrees(atan2(magn->y, magn->x));
}


/*
Returns the angular difference in the horizontal plane between the
"from" vector and north, in degrees.
Description of heading algorithm:
Shift and scale the magnetic reading based on calibration data to find
the North vector. Use the acceleration readings to determine the Up
vector (gravity is measured as an upward acceleration). The cross
product of North and Up vectors is East. The vectors East and North
form a basis for the horizontal plane. The From vector is projected
into the horizontal plane and the angle between the projected vector
and horizontal north is returned.
*/
static double heading(struct sensor_axis_t *magn,
                      struct sensor_axis_t *accel,
                      struct sensor_axis_t *from)
{
    struct sensor_axis_t temp_m = {magn->x, magn->y, magn->z};

    // compute E and N
    struct sensor_axis_t E;
    struct sensor_axis_t N;
    vector_cross(&temp_m, accel, &E);
    vector_normalize(&E);
    vector_cross(accel, &E, &N);
    vector_normalize(&N);

    // compute heading
    double heading = atan2(vector_dot(&E, from), vector_dot(&N, from)) * 180 / M_PI;
    return heading;
}


//void orientation_init(struct sensor_axis_t *accel, struct sensor_axis_t *magn)
//{
//}


void orientation_show(struct sensor_axis_t *accel,
                      struct sensor_axis_t *gyro,
                      struct sensor_axis_t *magn,
                      int pressure,
                      double temperature)
{
    double roll;
    double pitch;
    //double yaw_raw;
    double yaw;


    get_pitch_roll(accel, &roll, &pitch);

    struct sensor_axis_t from = {1,0,0};
    yaw = heading(magn, accel, &from);

    //get_yaw(magn, roll, pitch, &yaw_raw);



    //double rollRadians = to_radians(roll);
    //double pitchRadians = to_radians(pitch);
    //
    //double cosRoll = (float)cos(rollRadians);
    //double sinRoll = (float)sin(rollRadians);
    //double cosPitch = (float)cos(-1*pitchRadians);
    //double sinPitch = (float)sin(-1*pitchRadians);
    //
    ///* The tilt compensation algorithm                            */
    ///* Xh = X.cosPitch + Z.sinPitch                               */
    ///* Yh = X.sinRoll.sinPitch + Y.cosRoll - Z.sinRoll.cosPitch   */
    //magn->x = magn->x * cosPitch + magn->z * sinPitch;
    //magn->y = magn->x * sinRoll * sinPitch + magn->y * cosRoll - magn->z * sinRoll * cosPitch;



    //get_yaw(magn, roll, pitch, &yaw);



    static int print_rate_divider = 0;
    print_rate_divider++;
    if (print_rate_divider >= 6)
    {
        print_rate_divider = 0;
        fprintf(stdout, "% 7.2f % 7.2f % 7.2f ", roll, pitch, yaw);
        fprintf(stdout, "%8d %6.1f", pressure, temperature);
        fprintf(stdout, "\n");
    }
}
