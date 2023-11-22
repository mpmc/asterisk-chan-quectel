/*
    monitor_thread.h
*/

#ifndef CHAN_QUECTEL_MONITOR_THREAD_H_INCLUDED
#define CHAN_QUECTEL_MONITOR_THREAD_H_INCLUDED

struct pvt;

int pvt_monitor_start(struct pvt* pvt);
void pvt_monitor_stop(struct pvt* pvt);

#endif
