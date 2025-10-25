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

#ifndef _AM_PROFILER_CONFIG_H
#define _AM_PROFILER_CONFIG_H

#include <SparkyStudios/Audio/Amplitude/IO/Log.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Types.h>

namespace SparkyStudios::Audio::Amplitude
{
    /**
     * @brief Configuration for the profiler system.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerConfig
    {
        // Network settings
        bool mEnableNetworking;
        AmUInt16 mServerPort;
        AmUInt32 mMaxClients;
        AmString mBindAddress;

        // Update settings
        eProfilerUpdateMode mUpdateMode;
        AmReal32 mUpdateFrequencyHz;
        AmUInt32 mMaxMessagesPerFrame;

        // Data capture settings
        AmUInt32 mCategoryMask; // Bitmask of eProfilerCategory
        bool mCaptureEngineState;
        bool mCaptureEntityStates;
        bool mCaptureChannelStates;
        bool mCaptureListenerStates;
        bool mCapturePerformanceMetrics;
        bool mCaptureEvents;

        // Performance settings
        AmUInt32 mMessageBufferSize;
        AmUInt32 mMaxQueuedMessages;
        bool mUseCompressionForNetwork;

        // Filtering settings
        AmReal32 mPositionChangeThreshold; // Minimum position change to trigger update
        AmReal32 mOrientationChangeThreshold; // Minimum orientation change (radians)
        AmReal32 mParameterChangeThreshold; // Minimum parameter change percentage

        // Debug settings
        bool mEnableLogging;
        eLogMessageLevel mLoggingLevel;
        AmString mLogFilePath;

        /**
         * @brief Default constructor with sensible defaults
         */
        ProfilerConfig()
            : mEnableNetworking(true)
            , mServerPort(kDefaultProfilerPort)
            , mMaxClients(kMaxProfilerClients)
            , mBindAddress("127.0.0.1")
            , mUpdateMode(eProfilerUpdateMode_Timed)
            , mUpdateFrequencyHz(30.0f)
            , mMaxMessagesPerFrame(100)
            , mCategoryMask(static_cast<AmUInt32>(eProfilerCategory_All))
            , mCaptureEngineState(true)
            , mCaptureEntityStates(true)
            , mCaptureChannelStates(true)
            , mCaptureListenerStates(true)
            , mCapturePerformanceMetrics(true)
            , mCaptureEvents(true)
            , mMessageBufferSize(kProfilerMessageBufferSize)
            , mMaxQueuedMessages(1000)
            , mUseCompressionForNetwork(false)
            , mPositionChangeThreshold(0.01f) // 1cm
            , mOrientationChangeThreshold(0.017453f) // ~1 degree
            , mParameterChangeThreshold(0.01f) // 1%
            , mEnableLogging(false)
            , mLoggingLevel(eLogMessageLevel_Debug)
            , mLogFilePath("amplitude_profiler.log")
        {}

        /**
         * @brief Load configuration from JSON file
         */
        bool LoadFromFile(const AmOsString& configFile);

        /**
         * @brief Save configuration to JSON file
         */
        bool SaveToFile(const AmOsString& configFile) const;

        /**
         * @brief Validate configuration settings
         */
        bool Validate() const;
    };
} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_CONFIG_H
