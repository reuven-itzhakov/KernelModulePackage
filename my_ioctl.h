#ifndef MY_IOCTL_H
#define MY_IOCTL_H

#include <linux/ioctl.h>

struct my_driver_stats {
    int total_opens;
    int total_closes;
};

#define MY_IOC_MAGIC 'R'

#define MY_IOC_GET_STATS   _IOR(MY_IOC_MAGIC, 0, struct my_driver_stats)

#define MY_IOC_CLEAR_STATS _IO(MY_IOC_MAGIC, 1)

#endif