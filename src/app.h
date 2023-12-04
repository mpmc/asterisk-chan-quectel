/*
    app.h
*/

#ifndef CHAN_QUECTEL_APP_H_INCLUDED
#define CHAN_QUECTEL_APP_H_INCLUDED

#include "ast_config.h"

#ifdef BUILD_APPLICATIONS

int app_register();
void app_unregister();

#endif /* BUILD_APPLICATIONS */

#endif /* CHAN_QUECTEL_APP_H_INCLUDED */
