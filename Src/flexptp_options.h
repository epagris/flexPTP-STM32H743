#ifndef FLEXPTP_OPTIONS_H_
#define FLEXPTP_OPTIONS_H_

// INCLUDE appropriate port here!
#include "flexptp/hw_port/flexptp_options_stm32h743.h"

#define ANNOUNCE_COLLECTION_WINDOW (2)

#include "persistent_storage.h"

#define CONFIG_PTP (1588)
//#define PTP_CONFIG_PTR() ps_load(CONFIG_PTP)

#endif // FLEXPTP_OPTIONS_H_
