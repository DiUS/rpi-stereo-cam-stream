/* IIO - useful set of util functionality
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>

/* Made up value to limit allocation sizes */
#define IIO_MAX_NAME_LENGTH 30

#define FORMAT_SCAN_ELEMENTS_DIR "%s/scan_elements"
#define FORMAT_TYPE_FILE "%s_type"

/**
 * iioutils_break_up_name() - extract generic name from full channel name
 * @full_name: the full channel name
 * @generic_name: the output generic channel name
 *
 * Returns 0 on success, or a negative error code if string extraction failed.
 **/
int iioutils_break_up_name(const char *full_name, char **generic_name);

/**
 * struct iio_channel_info - information about a given channel
 * @name: channel name
 * @generic_name: general name for channel type
 * @scale: scale factor to be applied for conversion to si units
 * @offset: offset to be applied for conversion to si units
 * @index: the channel index in the buffer output
 * @bytes: number of bytes occupied in buffer output
 * @bits_used: number of valid bits of data
 * @shift: amount of bits to shift right data before applying bit mask
 * @mask: a bit mask for the raw output
 * @be: flag if data is big endian
 * @is_signed: is the raw value stored signed
 * @location: data offset for this channel inside the buffer (in bytes)
 **/
struct iio_channel_info {
	char *name;
	char *generic_name;
	float scale;
	float offset;
	unsigned index;
	unsigned bytes;
	unsigned bits_used;
	unsigned shift;
	uint64_t mask;
	unsigned be;
	unsigned is_signed;
	unsigned location;
};

/**
 * iioutils_get_type() - find and process _type attribute data
 * @is_signed: output whether channel is signed
 * @bytes: output how many bytes the channel storage occupies
 * @bits_used: output number of valid bits of data
 * @shift: output amount of bits to shift right data before applying bit mask
 * @mask: output a bit mask for the raw data
 * @be: output if data in big endian
 * @device_dir: the IIO device directory
 * @name: the channel name
 * @generic_name: the channel type name
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int iioutils_get_type(unsigned *is_signed,
		      unsigned *bytes,
		      unsigned *bits_used,
		      unsigned *shift,
		      uint64_t *mask,
		      unsigned *be,
		      const char *device_dir,
		      const char *name,
		      const char *generic_name);

/**
 * iioutils_get_param_float() - read a float value from a channel parameter
 * @output: output the float value
 * @param_name: the parameter name to read
 * @device_dir: the IIO device directory in sysfs
 * @name: the channel name
 * @generic_name: the channel type name
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int iioutils_get_param_float(float *output,
			     const char *param_name,
			     const char *device_dir,
			     const char *name,
			     const char *generic_name);

/**
 * size_from_channelarray() - calculate the storage size of a scan
 * @channels:		the channel info array
 * @num_channels:	number of channels
 *
 * Has the side effect of filling the channels[i].location values used
 * in processing the buffer output.
 **/
int size_from_channelarray(struct iio_channel_info *channels, int num_channels);

/**
 * bsort_channel_array_by_index() - sort the array in index order
 * @ci_array: the iio_channel_info array to be sorted
 * @cnt: the amount of array elements
 **/
void bsort_channel_array_by_index(struct iio_channel_info *ci_array, int cnt);

/**
 * build_channel_array() - function to figure out what channels are present
 * @device_dir: the IIO device directory in sysfs
 * @ci_array: output the resulting array of iio_channel_info
 * @counter: output the amount of array elements
 *
 * Returns 0 on success, otherwise a negative error code.
 **/
int build_channel_array(const char *device_dir,
			struct iio_channel_info **ci_array,
			int *counter);

/**
 * find_type_by_name() - function to match top level types by name
 * @name: top level type instance name
 * @type: the type of top level instance being searched
 *
 * Returns the device number of a matched IIO device on success, otherwise a
 * negative error code.
 * Typical types this is used for are device and trigger.
 **/
int find_type_by_name(const char *name, const char *type);

/**
 * write_sysfs_int() - write an integer value to a sysfs file
 * @filename: name of the file to write to
 * @basedir: the sysfs directory in which the file is to be found
 * @val: integer value to write to file
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int write_sysfs_int(const char *filename, char *basedir, int val);

/**
 * write_sysfs_int_and_verify() - write an integer value to a sysfs file
 *				  and verify
 * @filename: name of the file to write to
 * @basedir: the sysfs directory in which the file is to be found
 * @val: integer value to write to file
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int write_sysfs_int_and_verify(const char *filename, char *basedir, int val);

/**
 * write_sysfs_string_and_verify() - string write, readback and verify
 * @filename: name of file to write to
 * @basedir: the sysfs directory in which the file is to be found
 * @val: the string to write
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int write_sysfs_string_and_verify(const char *filename, char *basedir, char *val);

/**
 * write_sysfs_string() - write string to a sysfs file
 * @filename: name of file to write to
 * @basedir: the sysfs directory in which the file is to be found
 * @val: the string to write
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int write_sysfs_string(const char *filename, char *basedir, char *val);

/**
 * read_sysfs_posint() - read an integer value from file
 * @filename: name of file to read from
 * @basedir: the sysfs directory in which the file is to be found
 *
 * Returns the read integer value >= 0 on success, otherwise a negative error
 * code.
 **/
int read_sysfs_posint(const char *filename, char *basedir);

/**
 * read_sysfs_float() - read a float value from file
 * @filename: name of file to read from
 * @basedir: the sysfs directory in which the file is to be found
 * @val: output the read float value
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int read_sysfs_float(const char *filename, char *basedir, float *val);

/**
 * read_sysfs_string() - read a string from file
 * @filename: name of file to read from
 * @basedir: the sysfs directory in which the file is to be found
 * @str: output the read string
 *
 * Returns a value >= 0 on success, otherwise a negative error code.
 **/
int read_sysfs_string(const char *filename, const char *basedir, char *str);

extern const char *iio_dir;
