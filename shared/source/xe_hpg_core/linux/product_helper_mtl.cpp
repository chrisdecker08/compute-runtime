/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/linux/product_helper_xe_lpg_linux.inl"
#include "shared/source/xe_hpg_core/xe_lpg/os_agnostic_product_helper_xe_lpg.inl"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isCachingOnCpuAvailable() const {
    return false;
}
} // namespace NEO

template class NEO::ProductHelperHw<gfxProduct>;
