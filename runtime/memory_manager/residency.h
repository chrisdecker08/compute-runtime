/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cinttypes>
#include <vector>
namespace OCLRT {

constexpr uint32_t maxOsContextCount = 4u;

struct ResidencyData {
    ResidencyData() {
        std::fill_n(resident, sizeof(resident), false);
    }

    ~ResidencyData() = default;
    bool resident[maxOsContextCount];

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    std::vector<uint64_t> lastFenceValues;
};
} // namespace OCLRT
