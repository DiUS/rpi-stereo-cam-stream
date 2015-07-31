#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "calib.h"



static void consume_white_space(char **str)
{
    while (**str == ' ')
        (*str)++;
    int len = strlen(*str);
    if (len > 0)
    {
        char *tail = *str + len - 1;
        while ((*tail == ' ') && (tail > *str))
        {
            *tail = 0;
            tail--;
        }
    }
}


static void process_key_value(const char *key, const char *value,
                              struct calibration_data *accel,
                              struct calibration_data *magn,
                              struct calibration_data *gyro,
                              double *magnetic_declination_mrad)
{
    double v;
    errno = 0;
    v = strtod(value, NULL);
    if (errno == ERANGE)
    {
        fprintf(stderr, "Error: %s value out of range\n", key);
        return;
    }

    struct kv_map
    {
        const char *key;
        double *value;
    };
    struct kv_map map[] =
    {
        {"magn.declination_mrad", magnetic_declination_mrad},

        {"magn.x_offset",  &magn->x_offset },
        {"magn.y_offset",  &magn->y_offset },
        {"magn.z_offset",  &magn->z_offset },
        {"magn.x_scale",   &magn->x_scale  },
        {"magn.y_scale",   &magn->y_scale  },
        {"magn.z_scale",   &magn->z_scale  },

        {"accel.x_offset", &accel->x_offset},
        {"accel.y_offset", &accel->y_offset},
        {"accel.z_offset", &accel->z_offset},
        {"accel.x_scale",  &accel->x_scale },
        {"accel.y_scale",  &accel->y_scale },
        {"accel.z_scale",  &accel->z_scale },

        {"gyro.x_offset",  &gyro->x_offset },
        {"gyro.y_offset",  &gyro->y_offset },
        {"gyro.z_offset",  &gyro->z_offset },
        {"gyro.x_scale",   &gyro->x_scale  },
        {"gyro.y_scale",   &gyro->y_scale  },
        {"gyro.z_scale",   &gyro->z_scale  },
    };

    int i;
    for (i = 0; i < sizeof(map)/sizeof(struct kv_map); i++)
    {
        if (strcmp(map[i].key, key) == 0)
        {
            *map[i].value = v;
            return;
        }
    }
    fprintf(stderr, "Warning: unrecognized calibration key %s\n", key);
}


int read_calibration_from_file(const char *calibration_file,
                               struct calibration_data *accel,
                               struct calibration_data *magn,
                               struct calibration_data *gyro,
                               double *magnetic_declination_mrad)
{
#define STR_BUFFER_SIZE 128
    int ret = 0;
    FILE *stream;
    int len;
    char buffer[STR_BUFFER_SIZE];

    stream = fopen(calibration_file, "r");
    if (stream == NULL)
    {
        ret = -errno;
        fprintf(stderr, "Error: cannot open %s\n", calibration_file);
        return ret;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL)
    {
        len = strlen(buffer);
        // strip trailing '\n' if it exists
        if (buffer[len-1] == '\n')
        {
            buffer[len-1] = 0;
            len--;
        }
        else
        {
            // line is longer than our buffer, skip to the next line
            continue;
        }

        if (buffer[0] != '#')
        {
            // find the '=' sign
            char *equal = strstr(buffer, "=");
            if (equal)
            {
                char *key = buffer;
                char *value = equal+1;
                *equal = 0;

                consume_white_space(&key);
                process_key_value(key, value, accel, magn, gyro, magnetic_declination_mrad);
            }
        }
    }

    fclose(stream);
    return ret;
}
