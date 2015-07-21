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

#include "iio_utils.h"


struct iio_sensor_info
{
    char *sensor_name;
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
};

struct iio_trigger_info
{
    char *trigger_name;
    int trig_num;
    char *trig_dir_name;
};


// 10 Hz
#define IIO_SAMPLE_INTERVAL_NS  10000000
#define BUFFER_LENGTH           128


static char *barometric_path = "/sys/bus/i2c/drivers/bmp085/1-0077/pressure0_input";
static char *temperature_path = "/sys/bus/i2c/drivers/bmp085/1-0077/temp0_input";
static struct iio_sensor_info accel = { .sensor_name = "lsm303dlhc_accel", .dev_fd = -1 };
static struct iio_sensor_info magn  = { .sensor_name = "lsm303dlhc_magn", .dev_fd = -1 };
static struct iio_sensor_info gyro  = { .sensor_name = "l3gd20", .dev_fd = -1 };
static struct iio_trigger_info timer[] =
{
    { .trigger_name = "hrtimertrig0" },
    { .trigger_name = "hrtimertrig1" },
    { .trigger_name = "hrtimertrig2" },
};
static int terminated = 0;


#define min(a,b) ( (a < b) ? a : b )


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


static int setup_iio_device(struct iio_sensor_info *info)
{
    int ret;

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
    ret = build_channel_array(info->dev_dir_name, &info->channels, &info->num_channels);
    if (ret)
    {
        fprintf(stderr, "Problem reading %s scan element information\n", info->sensor_name);
        return ret;
    }

    info->scan_size = size_from_channelarray(info->channels, info->num_channels);
    info->data = malloc(info->scan_size * BUFFER_LENGTH);
    if (!info->data)
        return -ENOMEM;

    // Setup ring buffer parameters
    ret = write_sysfs_int("length", info->buf_dir_name, BUFFER_LENGTH);
    if (ret < 0)
        return ret;

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
    ret = write_sysfs_int("delay_ns", trigger->trig_dir_name, IIO_SAMPLE_INTERVAL_NS);
    if (ret < 0)
        return ret;
    return 0;
}


static int assign_trigger(struct iio_sensor_info *sensor, struct iio_trigger_info *trigger)
{
    int ret;

    fprintf(stderr, "%s - %s\n", sensor->dev_dir_name, trigger->trigger_name);
    ret = write_sysfs_string_and_verify("trigger/current_trigger",
                                        sensor->dev_dir_name,
                                        trigger->trigger_name);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to assign trigger %s to device %s\n", sensor->sensor_name, trigger->trigger_name);
        return ret;
    }
    return 0;
}


static int start_iio_device(struct iio_sensor_info *info)
{
    int ret;

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
    return write_sysfs_int("enable", info->buf_dir_name, 0);
}


static int disconnect_trigger(struct iio_sensor_info *info)
{
    return write_sysfs_string("trigger/current_trigger", info->dev_dir_name, "NULL");
}


static void clean_up_iio_device(struct iio_sensor_info *info)
{
    if (info->dev_fd >= 0)
    {
        close(info->dev_fd);
        info->dev_fd = -1;
    }
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






void print2byte(uint16_t input, struct iio_channel_info *info)
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
		printf("% 10.5f ", ((float)val + info->offset) * info->scale);
	} else {
		printf("% 10.5f ", ((float)input + info->offset) * info->scale);
	}
}

void print4byte(uint32_t input, struct iio_channel_info *info)
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
		printf("% 10.5f ", ((float)val + info->offset) * info->scale);
	} else {
		printf("% 10.5f ", ((float)input + info->offset) * info->scale);
	}
}

void print8byte(uint64_t input, struct iio_channel_info *info)
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
			printf("%" PRId64 " ", val);
		else
			printf("% 10.5f ",
			       ((float)val + info->offset) * info->scale);
	} else {
		printf("% 10.5f ", ((float)input + info->offset) * info->scale);
	}
}


/**
 * process_scan() - print out the values in SI units
 * @data:		pointer to the start of the scan
 * @channels:		information about the channels. Note
 *  size_from_channelarray must have been called first to fill the
 *  location offsets.
 * @num_channels:	number of channels
 **/
void process_scan(char *data,
		  struct iio_channel_info *channels,
		  int num_channels)
{
    int k;
    for (k = 0; k < num_channels; k++)
        switch (channels[k].bytes) {
            // only a few cases implemented so far
            case 2:
                print2byte(*(uint16_t *)(data + channels[k].location), &channels[k]);
                break;
            case 4:
                print4byte(*(uint32_t *)(data + channels[k].location), &channels[k]);
                break;
            case 8:
                print8byte(*(uint64_t *)(data + channels[k].location), &channels[k]);
                break;
            default:
                break;
        }
}


int main(int argc, char *argv[])
{
    int ret = 0;

    create_trigger(0);
    create_trigger(1);
    create_trigger(2);

    if ((ret = setup_iio_trigger(&timer[0])) != 0)
        goto error_ret;
    if ((ret = setup_iio_trigger(&timer[1])) != 0)
        goto error_ret;
    if ((ret = setup_iio_trigger(&timer[2])) != 0)
        goto error_ret;

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

    while (!terminated)
    {
        struct pollfd fds[] =
        {
            { .fd = accel.dev_fd, .events = POLLIN },
            { .fd =  magn.dev_fd, .events = POLLIN },
            { .fd =  gyro.dev_fd, .events = POLLIN },
        };
        poll(fds, sizeof(fds)/sizeof(struct pollfd), -1);

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
            }
        }

        int num_rows = BUFFER_LENGTH;
        for (i = 0; i < sizeof(fds)/sizeof(struct pollfd); i++)
        {
            struct iio_sensor_info *sensor;
            switch (i)
            {
                case 0: sensor = &accel; break;
                case 1: sensor =  &magn; break;
                case 2: sensor =  &gyro; break;
                default: continue;
            }
            if (sensor->read_size > 0)
                num_rows = min(num_rows, sensor->read_size/sensor->scan_size);
            else
                num_rows = 0;
        }
        if (num_rows)
        {
            int pressure = read_sensor_value(barometric_path);
            int raw_temperature = read_sensor_value(temperature_path);

            int j;
            for (j = 0; j < num_rows; j++)
            {
                for (i = 0; i < sizeof(fds)/sizeof(struct pollfd); i++)
                {
                    struct iio_sensor_info *sensor;
                    switch (i)
                    {
                        case 0: sensor = &accel; break;
                        case 1: sensor =  &magn; break;
                        case 2: sensor =  &gyro; break;
                        default: continue;
                    }
                    process_scan(sensor->data + sensor->scan_size * j,
				 sensor->channels,
				 sensor->num_channels);
                }
                printf("%8d %6.1f", pressure, ((double)raw_temperature)/10);
                printf("\n");
            }
        }
    }

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
