#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include <endian.h>
#include <inttypes.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>

#include "iio_utils.h"
#include "ahrs.h"
#include "calib.h"


struct iio_trigger_info
{
    char *trigger_name;
    int trig_num;
    char *trig_dir_name;
    int assigned;
};

struct iio_sensor_info
{
    char *sensor_name;
    int sampling_frequency;
    int iio_sample_interval_ms;
    char channel_index_to_axis_map[3];
    int invert_axes[3]; // x, y, z
    const char *sample_out_file;
    struct calibration_data *calibration;
    struct iio_channel_info *channels;
    int num_channels;
    int dev_num;
    int scan_size;
    int dev_fd;
    int read_size;
    char *dev_dir_name;
    char *buf_dir_name;
    char *buffer_access;
    char *data;
    struct iio_trigger_info *trigger;
};


#define BUFFER_LENGTH           128
#define MAX_PRINT_RATE_HZ       25


static char *barometric_path = "/sys/bus/i2c/drivers/bmp085/1-0077/pressure0_input";
static char *temperature_path = "/sys/bus/i2c/drivers/bmp085/1-0077/temp0_input";
static double magnetic_declination_mrad = 0;
static struct calibration_data accel_calibration =
{
    .x_offset = 0.263798,
    .y_offset = -0.053282,
    .z_offset = 0.103909,
    .x_scale  = 0.992941,
    .y_scale  = 0.995991,
    .z_scale  = 0.993166,
};
static struct calibration_data magn_calibration =
{
    .x_offset = -0.017075,
    .y_offset = -0.114040,
    .z_offset = 0.337632,
    .x_scale  = 1.610894,
    .y_scale  = 1.400538,
    .z_scale  = 1.763195,
};
static struct calibration_data gyro_calibration =
{
    .x_scale  = 1.0,
    .y_scale  = 1.0,
    .z_scale  = 1.0,
};
static struct iio_sensor_info accel =
{
    .sensor_name = "lsm303dlhc_accel",
    .sampling_frequency = 25,
    .iio_sample_interval_ms = 40,
    .channel_index_to_axis_map = {'x', 'y', 'z'},
    .calibration = &accel_calibration,
    .invert_axes = {0, 0, 1},
    .dev_fd = -1,
};
static struct iio_sensor_info magn  =
{
    .sensor_name = "lsm303dlhc_magn",
    .sampling_frequency = 30,
    .iio_sample_interval_ms = 33,
    .channel_index_to_axis_map = {'x', 'z', 'y'},
    .calibration = &magn_calibration,
    .invert_axes = {0, 0, 1},
    .dev_fd = -1,
};
static struct iio_sensor_info gyro  =
{
    .sensor_name = "l3gd20",
    .sampling_frequency = 95,
    .iio_sample_interval_ms = 11,
    .channel_index_to_axis_map = {'x', 'y', 'z'},
    .calibration = &gyro_calibration,
    .invert_axes = {1, 1, 1},
    .dev_fd = -1,
};
static struct iio_trigger_info timer[] =
{
    { .trigger_name = "hrtimertrig0" },
    { .trigger_name = "hrtimertrig1" },
    { .trigger_name = "hrtimertrig2" },
};
static int terminated = 0;
static const char *progname = "";
static const char *calibration_data_file = "/etc/default/rpi-stereo-cam-stream-calib.conf";
static int calibration_mode = 0;
static int raw_mode = 0;
static int apply_calibration_in_capture = 0;


#define min(a,b) ( (a < b) ? a : b )
#define max(a,b) ( (a > b) ? a : b )


static void handle_terminate_signal(int sig)
{
    if ((sig == SIGTERM) || (sig == SIGINT))
        terminated = 1;
}


static int enable_xyz_scan_channels(const char *device_dir)
{
    DIR *dp = NULL;
    const struct dirent *ent = NULL;
    char *scan_el_dir = NULL;
    int ret = 0;

    if (asprintf(&scan_el_dir, FORMAT_SCAN_ELEMENTS_DIR, device_dir) < 0)
    {
        ret = -ENOMEM;
        goto error_ret;
    }
    dp = opendir(scan_el_dir);
    if (dp == NULL)
    {
        ret = -errno;
        goto error_ret;
    }
    while (ent = readdir(dp), ent != NULL)
    {
        const char *d_name = ent->d_name + strlen(ent->d_name) - strlen("_x_en");
        if ((d_name >= ent->d_name) &&
            ((strcmp(d_name, "_x_en") == 0) ||
             (strcmp(d_name, "_y_en") == 0) ||
             (strcmp(d_name, "_z_en") == 0)))
        {
            ret = write_sysfs_int(ent->d_name, scan_el_dir, 1);
            if (ret < 0)
                goto error_ret;
        }
    }

error_ret:
    if (dp)
	closedir(dp);
    free(scan_el_dir);
    return ret;
}


static void apply_calibration_data(struct iio_channel_info *channels,
                                   int num_channels,
                                   struct calibration_data *calibration,
                                   char *channel_index_to_axis_map)
{
    if ((calibration) && (num_channels == 3))
    {
        int i;
        for (i = 0; i < num_channels; i++)
        {
            // Note: Do not use channels[i].name as it is wrong !!!
            switch (channel_index_to_axis_map[i])
            {
                case 'x':
                    channels[i].scale  *= calibration->x_scale;
                    channels[i].offset += calibration->x_offset / channels[i].scale;
                    break;
                case 'y':
                    channels[i].scale  *= calibration->y_scale;
                    channels[i].offset += calibration->y_offset / channels[i].scale;
                    break;
                case 'z':
                    channels[i].scale  *= calibration->z_scale;
                    channels[i].offset += calibration->z_offset / channels[i].scale;
                    break;
                default: return;
            }
            printf("%c offset %f, scale %f\n", channel_index_to_axis_map[i], channels[i].offset, channels[i].scale);
        }
    }
}


static int setup_iio_device(struct iio_sensor_info *info)
{
    int ret;

    if (calibration_mode && (info->sample_out_file == NULL))
        return 0;

    info->dev_num = find_type_by_name(info->sensor_name, "iio:device");
    if (info->dev_num < 0)
    {
        fprintf(stderr, "Failed to find device %s\n", info->sensor_name);
        return info->dev_num;
    }
    fprintf(stderr, "%s IIO device number: %d\n", info->sensor_name, info->dev_num);
    ret = asprintf(&info->dev_dir_name, "%siio:device%d", iio_dir, info->dev_num);
    if (ret < 0)
        return -ENOMEM;
    ret = asprintf(&info->buf_dir_name, "%s/buffer", info->dev_dir_name);
    if (ret < 0)
        return -ENOMEM;
    ret = asprintf(&info->buffer_access, "/dev/iio:device%d", info->dev_num);
    if (ret < 0)
        return -ENOMEM;

    ret = enable_xyz_scan_channels(info->dev_dir_name);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to enable %s scan elements\n", info->sensor_name);
        return ret;
    }
    ret = write_sysfs_int("sampling_frequency", info->dev_dir_name, info->sampling_frequency);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to set %s sampling frequency\n", info->sensor_name);
        return ret;
    }
    ret = build_channel_array(info->dev_dir_name, &info->channels, &info->num_channels);
    if (ret)
    {
        fprintf(stderr, "Problem reading %s scan element information\n", info->sensor_name);
        return ret;
    }
    if ((!calibration_mode) || apply_calibration_in_capture)
        apply_calibration_data(info->channels, info->num_channels, info->calibration, info->channel_index_to_axis_map);

    info->scan_size = size_from_channelarray(info->channels, info->num_channels);
    info->data = malloc(info->scan_size * BUFFER_LENGTH);
    if (!info->data)
        return -ENOMEM;

    // Setup ring buffer parameters
    ret = write_sysfs_int("length", info->buf_dir_name, BUFFER_LENGTH);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to set %s buffer length\n", info->sensor_name);
        return ret;
    }

    return 0;
}


static int create_trigger(int trigger_id)
{
    char *add_trigger_dir = NULL;
    int ret;

    ret = asprintf(&add_trigger_dir, "%siio_hrtimer_trigger", iio_dir);
    if (ret < 0)
        return -ENOMEM;

    ret = write_sysfs_int("add_trigger", add_trigger_dir, trigger_id);
    free(add_trigger_dir);
    if (ret < 0)
        return ret;
    return 0;
}


static int setup_iio_trigger(struct iio_trigger_info *trigger)
{
    int ret;

    trigger->trig_num = find_type_by_name(trigger->trigger_name, "trigger");
    if (trigger->trig_num < 0)
    {
        fprintf(stderr, "Failed to find trigger %s\n", trigger->trigger_name);
        return trigger->trig_num;
    }
    fprintf(stderr, "%s IIO trigger number: %d\n", trigger->trigger_name, trigger->trig_num);
    ret = asprintf(&trigger->trig_dir_name, "%strigger%d", iio_dir, trigger->trig_num);
    if (ret < 0)
        return -ENOMEM;
    return 0;
}


static int assign_trigger(struct iio_sensor_info *sensor, struct iio_trigger_info *trigger)
{
    int ret;

    if (trigger->assigned)
        return -1;

    if (calibration_mode && (sensor->sample_out_file == NULL))
        return 0;

    fprintf(stderr, "%s - %s\n", sensor->dev_dir_name, trigger->trigger_name);
    ret = write_sysfs_string_and_verify("trigger/current_trigger",
                                        sensor->dev_dir_name,
                                        trigger->trigger_name);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to assign trigger %s to device %s\n", sensor->sensor_name, trigger->trigger_name);
        return ret;
    }
    ret = write_sysfs_int("delay_ns", trigger->trig_dir_name, sensor->iio_sample_interval_ms * 1000000);
    if (ret < 0)
        return ret;
    trigger->assigned = 1;
    sensor->trigger = trigger;
    return 0;
}


static int start_iio_device(struct iio_sensor_info *info)
{
    int ret;

    if (calibration_mode && (info->sample_out_file == NULL))
        return 0;

    // Enable the buffer
    ret = write_sysfs_int("enable", info->buf_dir_name, 1);
    if (ret < 0)
        return ret;

    // Attempt to open non blocking the access dev
    info->dev_fd = open(info->buffer_access, O_RDONLY | O_NONBLOCK);
    if (info->dev_fd == -1)
    {
        ret = -errno;
        fprintf(stderr, "Failed to open %s\n", info->buffer_access);
        return ret;
    }

    return 0;
}


static int stop_iio_device(struct iio_sensor_info *info)
{
    if (calibration_mode && (info->sample_out_file == NULL))
        return 0;
    return write_sysfs_int("enable", info->buf_dir_name, 0);
}


static int disconnect_trigger(struct iio_sensor_info *info)
{
    if (info->trigger == NULL)
        return -1;
    info->trigger->assigned = 0;
    info->trigger = NULL;
    return write_sysfs_string("trigger/current_trigger", info->dev_dir_name, "NULL");
}


static void clean_up_iio_device(struct iio_sensor_info *info)
{
    close(info->dev_fd);
    info->dev_fd = -1;
    free(info->channels);
    info->channels = NULL;
    free(info->data);
    info->data = NULL;
    free(info->dev_dir_name);
    info->dev_dir_name = NULL;
    free(info->buf_dir_name);
    info->buf_dir_name = NULL;
    free(info->buffer_access);
    info->buffer_access = NULL;
}


static void clean_up_iio_trigger(struct iio_trigger_info *trigger)
{
    free(trigger->trig_dir_name);
    trigger->trig_dir_name = NULL;
}


static int read_sensor_value(char *path)
{
    int fd;
    int value = -1;
    char tmp_buf[16];

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if ((fd != -1) &&
        (read(fd, tmp_buf, sizeof(tmp_buf)) > 0))
        value = atoi(tmp_buf);
    if (fd != -1)
        close(fd);
    return value;
}

//------------------------------------------------------------------------------


static double double2byte(uint16_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be16toh(input);
    else
        input = le16toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int16_t val = (int16_t)(input << (16 - info->bits_used)) >>
                      (16 - info->bits_used);
        //printf("--> %f, %f, %f, %f\n", (double)val, info->offset, info->scale, info->offset*info->scale);
        return ((double)val + info->offset) * info->scale;
    } else {
        return ((double)input + info->offset) * info->scale;
    }
}

static double double4byte(uint32_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be32toh(input);
    else
        input = le32toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int32_t val = (int32_t)(input << (32 - info->bits_used)) >>
                      (32 - info->bits_used);
        return ((double)val + info->offset) * info->scale;
    } else {
        return ((double)input + info->offset) * info->scale;
    }
}

static double double8byte(uint64_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be64toh(input);
    else
        input = le64toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int64_t val = (int64_t)(input << (64 - info->bits_used)) >>
                      (64 - info->bits_used);
        /* special case for timestamp */
        if (info->scale == 1.0f && info->offset == 0.0f)
            return (double)val;
        else
            return ((double)val + info->offset) * info->scale;
    } else {
        return ((double)input + info->offset) * info->scale;
    }
}


static void populate_sensor_axis(char *data,
		                 struct iio_channel_info *channels,
		                 int num_channels,
                                 char *channel_index_to_axis_map,
                                 int *invert_axis,
                                 struct sensor_axis_t *axis)
{
    int k;
    if (num_channels != 3)
        return;
    for (k = 0; k < num_channels; k++)
    {
        double *a;
        int invert = 0;
        switch (channel_index_to_axis_map[k])
        {
            case 'x': a = &axis->x; invert = invert_axis[0]; break;
            case 'y': a = &axis->y; invert = invert_axis[1]; break;
            case 'z': a = &axis->z; invert = invert_axis[2]; break;
            default: a = NULL;
        }
        if (a == NULL)
            break;
        switch (channels[k].bytes)
        {
            // only a few cases implemented so far
            case 2:
                *a = double2byte(*(uint16_t *)(data + channels[k].location), &channels[k]);
                break;
            case 4:
                *a = double4byte(*(uint32_t *)(data + channels[k].location), &channels[k]);
                break;
            case 8:
                *a = double8byte(*(uint64_t *)(data + channels[k].location), &channels[k]);
                break;
            default:
                break;
        }
        if (invert)
            *a *= (-1);
    }
}


//------------------------------------------------------------------------------

static void print_raw_axis(FILE *fp, struct sensor_axis_t *axis)
{
    fprintf(fp, "% 10.5f % 10.5f % 10.5f ", axis->x, axis->y, axis->z);
}


static void process_samples(void)
{
    struct sensor_axis_t accel_axis;
    struct sensor_axis_t magn_axis;
    struct sensor_axis_t gyro_axis;
    int pressure = read_sensor_value(barometric_path);
    int raw_temperature = read_sensor_value(temperature_path);
    struct timespec pressure_sample_time;
    clock_gettime(CLOCK_MONOTONIC, &pressure_sample_time);

    while (!terminated)
    {
        struct pollfd fds[] =
        {
            { .fd = accel.dev_fd, .events = POLLIN },
            { .fd =  magn.dev_fd, .events = POLLIN },
            { .fd =  gyro.dev_fd, .events = POLLIN },
        };
        poll(fds, sizeof(fds)/sizeof(struct pollfd), -1);

        int num_rows = 0;
        int i;
        for (i = 0; i < sizeof(fds)/sizeof(struct pollfd); i++)
        {
            if ((fds[i].revents & POLLIN) != 0)
            {
                struct iio_sensor_info *sensor;
                switch (i)
                {
                    case 0: sensor = &accel; break;
                    case 1: sensor =  &magn; break;
                    case 2: sensor =  &gyro; break;
                    default: continue;
                }
                sensor->read_size = read(sensor->dev_fd, sensor->data, BUFFER_LENGTH*sensor->scan_size);
		if (sensor->read_size < 0)
                {
                    if (errno == EAGAIN)
                        continue;
                    else
                    {
                        terminated = 1;
                        break;
                    }
		}
                num_rows = max(sensor->read_size/sensor->scan_size, num_rows);
            }
        }

        int accel_div = 1;
        int magn_div = 1;
        int gyro_div = 1;
        for (i = 0; i < sizeof(fds)/sizeof(struct pollfd); i++)
        {
            int *div = NULL;
            struct iio_sensor_info *sensor;
            switch (i)
            {
                case 0: sensor = &accel; div = &accel_div; break;
                case 1: sensor =  &magn; div = &magn_div;  break;
                case 2: sensor =  &gyro; div = &gyro_div;  break;
                default: continue;
            }
            if (sensor->read_size)
                *div = num_rows * sensor->scan_size / sensor->read_size;
        }

        // Read barometric and temperature
        struct timespec now;
        if ((clock_gettime(CLOCK_MONOTONIC, &now) == 0) &&
            (now.tv_sec - pressure_sample_time.tv_sec > 1))
        {
            pressure_sample_time = now;
            pressure = read_sensor_value(barometric_path);
            raw_temperature = read_sensor_value(temperature_path);
        }

        struct sensor_axis_t *axis;
        int accel_count = 0;
        int magn_count = 0;
        int gyro_count = 0;
        int accel_read_idx = 0;
        int magn_read_idx = 0;
        int gyro_read_idx = 0;
        int j;
        for (j = 0; j < num_rows; j++)
        {
            for (i = 0; i < sizeof(fds)/sizeof(struct pollfd); i++)
            {
                int *count = NULL;
                int *div = NULL;
                int *read_idx = NULL;
                struct iio_sensor_info *sensor;
                switch (i)
                {
                    case 0:
                        sensor = &accel;
                        axis = &accel_axis;
                        count = &accel_count;
                        div = &accel_div;
                        read_idx = &accel_read_idx;
                        break;
                    case 1:
                        sensor = &magn;
                        axis = &magn_axis;
                        count = &magn_count;
                        div = &magn_div;
                        read_idx = &magn_read_idx;
                        break;
                    case 2:
                        sensor = &gyro;
                        axis = &gyro_axis;
                        count = &gyro_count;
                        div = &gyro_div;
                        read_idx = &gyro_read_idx;
                        break;
                    default:
                        continue;
                }
                (*count)++;
                if (*count >= *div)
                {
                    *count = 0;
                    if (*read_idx < sensor->read_size)
                    {
                        populate_sensor_axis(sensor->data + sensor->scan_size * (*read_idx),
                                             sensor->channels,
                                             sensor->num_channels,
                                             sensor->channel_index_to_axis_map,
                                             sensor->invert_axes,
                                             axis);
                        (*read_idx)++;
                    }
                }
            }

            if (raw_mode)
            {
                static int print_rate_divider = 0;
                print_rate_divider++;
                if (print_rate_divider >= 8)
                {
                    print_rate_divider = 0;
                    print_raw_axis(stdout, &accel_axis);
                    print_raw_axis(stdout, &magn_axis);
                    print_raw_axis(stdout, &gyro_axis);
                    fprintf(stdout, "%8d %6.1f", pressure, ((double)raw_temperature)/10);
                    fprintf(stdout, "\n");
                }
            }
            else
                orientation_show(&accel_axis, &gyro_axis, &magn_axis, magnetic_declination_mrad, pressure, ((double)raw_temperature)/10);
        }
    }
}


static int calibrate_sensor(struct iio_sensor_info *sensor)
{
    if (sensor->sample_out_file == NULL)
        return 0;

    int print_rate_divider = 1;
    int print_rate_counter = 0;
    while (sensor->iio_sample_interval_ms * print_rate_divider < 1000 / MAX_PRINT_RATE_HZ)
        print_rate_divider++;

    int ret = 0;
    int num_lines = 0;
    FILE *fp = fopen(sensor->sample_out_file, "w");
    if (fp == NULL)
    {
        ret = -errno;
        fprintf(stderr, "Failed to open %s\n", sensor->sample_out_file);
        return ret;
    }

    while (!terminated)
    {
        struct pollfd fdp =
        {
            .fd = sensor->dev_fd,
            .events = POLLIN
        };
        poll(&fdp, 1, -1);

        int i;
        if ((fdp.revents & POLLIN) != 0)
        {
            sensor->read_size = read(sensor->dev_fd, sensor->data, BUFFER_LENGTH*sensor->scan_size);
            if (sensor->read_size < 0)
            {
                if (errno == EAGAIN)
                    continue;
                else
                {
                    ret = -errno;
                    terminated = 1;
                    break;
                }
            }
            struct sensor_axis_t axis;
            int num_rows = sensor->read_size/sensor->scan_size;
            int j;
            for (j = 0; j < num_rows; j++)
            {
                print_rate_counter++;
                if (print_rate_counter >= print_rate_divider)
                {
                    print_rate_counter = 0;
                    populate_sensor_axis(sensor->data + sensor->scan_size * j,
                                         sensor->channels,
                                         sensor->num_channels,
                                         sensor->channel_index_to_axis_map,
                                         sensor->invert_axes,
                                         &axis);
                    print_raw_axis(stdout, &axis);
                    fprintf(stdout, "\n");
                    print_raw_axis(fp, &axis);
                    fprintf(fp, "\n");

                    num_lines++;
                    if (num_lines >= 0xFFFF)
                        terminated = 1;
                }
            }
        }
    }

    fclose(fp);
    return ret;
}

//------------------------------------------------------------------------------

void syntax(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s [options]\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -M <path>     Calibrate magnetometer mode, write samples to <path>\n");
    fprintf(stderr, " -A <path>     Calibrate accelerometer mode, write samples to <path>\n");
    fprintf(stderr, " -G <path>     Calibrate gyroscope mode, write samples to <path>\n");
    fprintf(stderr, " -C            Apply calibration data in calibration mode\n");
    fprintf(stderr, " -c <path>     Calibration data (default %s)\n", calibration_data_file);
    fprintf(stderr, " -r            Raw data mode\n");
    fprintf(stderr, " -h            display this information\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "When calibrating more than one sensor, the magnetometer calibration will run\n"
                    "first, followed by accelerometer, then gyroscope. Hit ctrl-C to complete the\n"
                    "current calibration.\n");
    fprintf(stderr, "How to calibrate magnetometer:\n"
                    "\t- Rotate sensor around the XYZ axes slowly\n");
    fprintf(stderr, "How to calibrate accelerometer:\n"
                    "\t- Rotate sensor around the XYZ axes slowly and GENTLY\n");
    fprintf(stderr, "How to calibrate gyroscope:\n"
                    "\t- Leave the sensor on a stable surface for a short while\n");
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    int ret = 0;
    int opt;

    progname = argv[0];

    while ((opt = getopt (argc, argv, "M:A:G:c:Crh")) != -1)
    {
        switch (opt)
        {
            case 'A': accel.sample_out_file = optarg; if (strlen(accel.sample_out_file) == 0) syntax(); break;
            case 'M': magn.sample_out_file = optarg; if (strlen(magn.sample_out_file) == 0) syntax(); break;
            case 'G': gyro.sample_out_file = optarg; if (strlen(gyro.sample_out_file) == 0) syntax(); break;
            case 'c': calibration_data_file = optarg; if (strlen(calibration_data_file) == 0) syntax(); break;
            case 'r': raw_mode = 1; break;
            case 'C': apply_calibration_in_capture = 1; break;
            case 'h': // fall through
            default:
                syntax();
                break;
        }
    }

    if (read_calibration_from_file(calibration_data_file, &accel_calibration,
                                   &magn_calibration, &gyro_calibration,
                                   &magnetic_declination_mrad))
        fprintf(stderr, "Warning: no calibration data available\n");

    if ((accel.sample_out_file != NULL) ||
        (magn.sample_out_file != NULL) ||
        (gyro.sample_out_file != NULL))
        calibration_mode = 1;

    if ((!calibration_mode) || accel.sample_out_file)
        create_trigger(0);
    if ((!calibration_mode) || magn.sample_out_file)
        create_trigger(1);
    if ((!calibration_mode) || gyro.sample_out_file)
        create_trigger(2);

    if ((!calibration_mode) || accel.sample_out_file)
    {
        if ((ret = setup_iio_trigger(&timer[0])) != 0)
            goto error_ret;
    }
    if ((!calibration_mode) || magn.sample_out_file)
    {
        if ((ret = setup_iio_trigger(&timer[1])) != 0)
            goto error_ret;
    }
    if ((!calibration_mode) || gyro.sample_out_file)
    {
        if ((ret = setup_iio_trigger(&timer[2])) != 0)
            goto error_ret;
    }

    if (((ret = setup_iio_device(&accel)) != 0) ||
        ((ret = setup_iio_device(&magn)) != 0) ||
        ((ret = setup_iio_device(&gyro)) != 0))
        goto error_ret;

    if (((ret = assign_trigger(&accel, &timer[0])) != 0) ||
        ((ret = assign_trigger(&magn, &timer[1])) != 0) ||
        ((ret = assign_trigger(&gyro, &timer[2])) != 0))
        goto error_ret;

    signal(SIGINT, handle_terminate_signal);
    signal(SIGTERM, handle_terminate_signal);

    if (((ret = start_iio_device(&accel)) != 0) ||
        ((ret = start_iio_device(&magn)) != 0) ||
        ((ret = start_iio_device(&gyro)) != 0))
        goto error_ret;

    if (calibration_mode)
    {
        if (!terminated)
        {
            fprintf(stdout, "\n");
            fprintf(stdout, "***********************************************\n");
            fprintf(stdout, "          M A G N E T O M E T E R\n");
            fprintf(stdout, "***********************************************\n");
            if (calibrate_sensor(&magn) == 0)
                terminated = 0;
        }
        if (!terminated)
        {
            fprintf(stdout, "\n");
            fprintf(stdout, "***********************************************\n");
            fprintf(stdout, "         A C C E L E R O M E T E R\n");
            fprintf(stdout, "***********************************************\n");
            if (calibrate_sensor(&accel) == 0)
                terminated = 0;
        }
        if (!terminated)
        {
            fprintf(stdout, "\n");
            fprintf(stdout, "***********************************************\n");
            fprintf(stdout, "             G Y R O S C O P E\n");
            fprintf(stdout, "***********************************************\n");
            if (calibrate_sensor(&gyro) == 0)
                terminated = 0;
        }
    }
    else
        process_samples();

    stop_iio_device(&accel);
    stop_iio_device(&magn);
    stop_iio_device(&gyro);
    disconnect_trigger(&accel);
    disconnect_trigger(&magn);
    disconnect_trigger(&gyro);

error_ret:
    clean_up_iio_device(&accel);
    clean_up_iio_device(&magn);
    clean_up_iio_device(&gyro);
    clean_up_iio_trigger(&timer[0]);
    clean_up_iio_trigger(&timer[1]);
    clean_up_iio_trigger(&timer[2]);
    return ret;
}
