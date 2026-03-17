#pragma once

#include "esp_err.h"
#include "hap.h"

esp_err_t hk_pairing_info_print(const char *accessory_name,
                                const char *setup_code,
                                const char *setup_id,
                                hap_cid_t cid);
