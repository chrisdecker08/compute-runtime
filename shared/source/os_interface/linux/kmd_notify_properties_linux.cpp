/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"

using namespace NEO;

void KmdNotifyHelper::updateAcLineStatus() {}

int64_t KmdNotifyHelper::getBaseTimeout() const {
    return properties->delayKmdNotifyMicroseconds;
}
