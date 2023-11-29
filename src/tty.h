/*
    tty.h
*/
#ifndef CHAN_QUECTEL_TTY_H_INCLUDED
#define CHAN_QUECTEL_TTY_H_INCLUDED

int tty_open(const char* dev, int typ);
void tty_close_lck(const char* dev, int fd, int exclusive, int flck);
void tty_close(const char* dev, int fd);
int tty_status(int fd, int* err);

#endif
