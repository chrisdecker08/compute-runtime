/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/xe/xedrm.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/unit_test/os_interface/linux/xe/mock_ioctl_helper_xe.h"
namespace NEO {
namespace XeDrm {
#include "shared/test/unit_test/os_interface/linux/xe/mock_drm_xe.inl"
}
} // namespace NEO
using NEO::XeDrm::DrmMockXe;
