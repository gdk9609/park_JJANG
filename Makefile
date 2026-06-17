CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE
TARGET=parking_server
SRCS=server.c admin.c tower.c reservation.c parking.c fee.c payment.c filedb.c utils.c time_utils.c
OBJS=$(SRCS:.c=.o)
LDFLAGS=-pthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c parking.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

reset-data:
	rm -rf data
	mkdir -p data
