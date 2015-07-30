ifneq ($(SRC),)
VPATH=$(SRC)
endif

CFLAGS += -I. -I$(SRC) -Wall -std=c99 -D_BSD_SOURCE=1 -D_GNU_SOURCE=1
LDFLAGS += -lm -lrt

all: test_iio_sensors lsiio generic_buffer

test_iio_sensors: test_iio_sensors.o iio_utils.o calib.o ahrs.o
	$(CC) $^ $(LDFLAGS) -o $@

lsiio: lsiio.o iio_utils.o
	$(CC) $^ $(LDFLAGS) -o $@

generic_buffer: generic_buffer.o iio_utils.o
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o test_iio_sensors lsiio generic_buffer
