#ifndef _CALIB_H_
#define _CALIB_H_


struct calibration_data
{
    double x_offset;
    double y_offset;
    double z_offset;
    double x_scale;
    double y_scale;
    double z_scale;
};


int read_calibration_from_file(const char *calibration_file,
                               struct calibration_data *accel,
                               struct calibration_data *magn,
                               struct calibration_data *gyro,
                               double *magnetic_declination_mrad);


#endif // _CALIB_H_
