/*
    eventfd.h
*/

#ifndef CHAN_QUECTEL_EVENTFD_H_INCLUDED
#define CHAN_QUECTEL_EVENTFD_H_INCLUDED

int eventfd_create();
int eventfd_set(int fd);
int eventfd_reset(int fd);
void eventfd_close(int* fd);

#endif
