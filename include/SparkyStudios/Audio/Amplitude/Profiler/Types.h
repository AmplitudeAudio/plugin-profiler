// Copyright (c) 2025-present Sparky Studios. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifndef _AM_PROFILER_TYPES_H
#define _AM_PROFILER_TYPES_H

#include <SparkyStudios/Audio/Amplitude/Core/Common.h>
#include <SparkyStudios/Audio/Amplitude/Math/Utils.h>

#include <chrono>

namespace SparkyStudios::Audio::Amplitude
{
    // Forward declarations
    class Entity;
    class Channel;
    class Listener;

    /**
     * @brief Profiler time type
     *
     * @ingroup profiling
     */
    using ProfilerTime = std::chrono::high_resolution_clock::time_point;

    /**
     * @brief Profiler client ID type
     *
     * @ingroup profiling
     */
    using ProfilerClientID = AmUInt32;

    /**
     * @brief Profiler message ID type
     *
     * @ingroup profiling
     */
    using ProfilerMessageID = AmUInt64;

    /**
     * @brief Default port for the profiler
     *
     * @ingroup profiling
     */
    constexpr AmUInt16 kDefaultProfilerPort = 27002;

    /**
     * @brief Maximum number of profiler clients
     *
     * @ingroup profiling
     */
    constexpr AmUInt32 kMaxProfilerClients = 8;

    /**
     * @brief Size of the profiler message buffer, in bytes
     *
     * @ingroup profiling
     */
    constexpr AmUInt32 kProfilerMessageBufferSize = 1024 * 1024; // 1MB buffer

    /**
     * @brief Profiler update frequency modes
     *
     * @ingroup profiling
     */
    enum eProfilerUpdateMode : AmUInt8
    {
        /**
         * @brief Updates are sent at fixed intervals
         */
        eProfilerUpdateMode_Timed = 0,

        /**
         * @brief Updates are sent when significant changes occur
         */
        eProfilerUpdateMode_OnChange = 1,

        /**
         * @brief Updates are sent every engine update cycle
         */
        eProfilerUpdateMode_PerFrame = 2,

        /**
         * @brief Manual updates only (via explicit calls)
         */
        eProfilerUpdateMode_Manual = 3
    };

    /**
     * @brief Profiler message categories for filtering
     *
     * @ingroup profiling
     */
    enum eProfilerCategory : AmUInt32
    {
        /**
         * @brief No categories, or unknown category selected
         */
        eProfilerCategory_None = 0,

        /**
         * @brief Engine-related messages
         */
        eProfilerCategory_Engine = 1 << 0,

        /**
         * @brief Entity-related messages
         */
        eProfilerCategory_Entity = 1 << 1,

        /**
         * @brief Channel-related messages
         */
        eProfilerCategory_Channel = 1 << 2,

        /**
         * @brief Listener-related messages
         */
        eProfilerCategory_Listener = 1 << 3,

        /**
         * @brief Environment-related messages
         */
        eProfilerCategory_Environment = 1 << 4,

        /**
         * @brief Performance-related messages
         */
        eProfilerCategory_Performance = 1 << 5,

        /**
         * @brief Memory-related messages
         */
        eProfilerCategory_Memory = 1 << 6,

        /**
         * @brief Event-related messages
         */
        eProfilerCategory_Events = 1 << 7,

        /**
         * @brief All categories combined
         */
        eProfilerCategory_All = 0xFFFFFFFF
    };

    /**
     * @brief Profiler message priority levels
     *
     * @ingroup profiling
     */
    enum eProfilerPriority : AmUInt8
    {
        /**
         * @brief Low priority messages
         */
        eProfilerPriority_Low = 0,

        /**
         * @brief Normal priority messages
         */
        eProfilerPriority_Normal = 1,

        /**
         * @brief High priority messages
         */
        eProfilerPriority_High = 2,

        /**
         * @brief Critical priority messages
         */
        eProfilerPriority_Critical = 3
    };
} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_TYPES_H
