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

#ifndef _AM_PROFILER_DATA_COLLECTOR_H
#define _AM_PROFILER_DATA_COLLECTOR_H

#include <SparkyStudios/Audio/Amplitude/Core/Common.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>

#include <vector>

namespace SparkyStudios::Audio::Amplitude
{
    // Forward declarations
    class Engine;
    class Entity;
    class Channel;
    class Listener;

    /**
     * @brief Collects profiling data from the Amplitude engine.
     *
     * This class is responsible for gathering real-time data from various
     * components of the audio engine and converting them into structured
     * profiler data snapshots.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerDataCollector
    {
    public:
        /**
         * @brief Default constructor.
         */
        ProfilerDataCollector();

        /**
         * @brief Destructor.
         */
        ~ProfilerDataCollector();

        /**
         * @brief Initialize the data collector.
         *
         * @return true if initialization was successful, false otherwise.
         */
        bool Initialize();

        /**
         * @brief Deinitialize the data collector.
         */
        void Deinitialize();

        /**
         * @brief Check if the data collector is initialized.
         *
         * @return true if initialized, false otherwise.
         */
        bool IsInitialized() const;

        // Data collection methods

        /**
         * @brief Collect current engine state data.
         *
         * @return ProfilerEngineData snapshot of the current engine state.
         */
        ProfilerEngineData CollectEngineData() const;

        /**
         * @brief Collect data for a specific entity.
         *
         * @param entityId The ID of the entity to collect data for.
         * @return ProfilerEntityData snapshot of the entity state.
         */
        ProfilerEntityData CollectEntityData(AmEntityID entityId) const;

        /**
         * @brief Collect data for a specific channel.
         *
         * @param channelId The ID of the channel to collect data for.
         * @return ProfilerChannelData snapshot of the channel state.
         */
        ProfilerChannelData CollectChannelData(AmChannelID channelId) const;

        /**
         * @brief Collect data for a specific listener.
         *
         * @param listenerId The ID of the listener to collect data for.
         * @return ProfilerListenerData snapshot of the listener state.
         */
        ProfilerListenerData CollectListenerData(AmListenerID listenerId) const;

        /**
         * @brief Collect current performance metrics.
         *
         * @return ProfilerPerformanceData snapshot of performance metrics.
         */
        ProfilerPerformanceData CollectPerformanceData() const;

        // Bulk collection helpers

        /**
         * @brief Get IDs of all active entities.
         *
         * @return Vector of active entity IDs.
         */
        std::vector<AmEntityID> GetAllEntityIds() const;

        /**
         * @brief Get IDs of all active channels.
         *
         * @return Vector of active channel IDs.
         */
        std::vector<AmChannelID> GetAllChannelIds() const;

        /**
         * @brief Get IDs of all active listeners.
         *
         * @return Vector of active listener IDs.
         */
        std::vector<AmListenerID> GetAllListenerIds() const;

        /**
         * @brief Collect data for all active entities.
         *
         * @return Vector of ProfilerEntityData for all active entities.
         */
        std::vector<ProfilerEntityData> CollectAllEntityData() const;

        /**
         * @brief Collect data for all active channels.
         *
         * @return Vector of ProfilerChannelData for all active channels.
         */
        std::vector<ProfilerChannelData> CollectAllChannelData() const;

        /**
         * @brief Collect data for all active listeners.
         *
         * @return Vector of ProfilerListenerData for all active listeners.
         */
        std::vector<ProfilerListenerData> CollectAllListenerData() const;

        // Performance monitoring helpers

        /**
         * @brief Get current memory usage in bytes.
         *
         * @return Current memory usage.
         */
        AmUInt64 GetCurrentMemoryUsage() const;

        /**
         * @brief Get peak memory usage in bytes.
         *
         * @return Peak memory usage since engine initialization.
         */
        AmUInt64 GetPeakMemoryUsage() const;

        /**
         * @brief Get current CPU usage percentage.
         *
         * @return CPU usage as a percentage (0.0-100.0).
         */
        AmReal32 GetCurrentCpuUsage() const;

        /**
         * @brief Get the number of active audio voices.
         *
         * @return Number of currently active voices.
         */
        AmUInt32 GetActiveVoiceCount() const;

        /**
         * @brief Get the maximum number of concurrent voices supported.
         *
         * @return Maximum voice count.
         */
        AmUInt32 GetMaxVoiceCount() const;

        /**
         * @brief Get list of loaded plugins.
         *
         * @return Vector of plugin names currently loaded.
         */
        std::vector<AmString> GetLoadedPlugins() const;

        /**
         * @brief Get list of loaded sound banks.
         *
         * @return Vector of sound bank names currently loaded.
         */
        std::vector<AmString> GetLoadedSoundBanks() const;

        /**
         * @brief Get counts of various asset types.
         *
         * @return Map of asset type names to their counts.
         */
        std::unordered_map<AmString, AmUInt32> GetAssetCounts() const;

    private:
        // Helper methods for specific data collection

        /**
         * @brief Calculate distance between entity and listener.
         *
         * @param entityId The entity ID.
         * @param listenerId The listener ID (optional, uses default if not specified).
         * @return Distance in world units.
         */
        AmReal32 CalculateDistanceToListener(AmEntityID entityId, AmListenerID listenerId = kAmInvalidObjectId) const;

        /**
         * @brief Calculate attenuation factor for an entity.
         *
         * @param entityId The entity ID.
         * @return Attenuation factor (0.0-1.0).
         */
        AmReal32 CalculateAttenuationFactor(AmEntityID entityId) const;

        /**
         * @brief Get azimuth and elevation relative to listener.
         *
         * @param entityId The entity ID.
         * @param azimuth [out] Azimuth angle in radians.
         * @param elevation [out] Elevation angle in radians.
         */
        void CalculateSphericalPosition(AmEntityID entityId, AmReal32& azimuth, AmReal32& elevation) const;

        /**
         * @brief Collect active effects for a channel.
         *
         * @param channelId The channel ID.
         * @return Vector of effect names.
         */
        std::vector<AmString> CollectChannelEffects(AmChannelID channelId) const;

        /**
         * @brief Collect effect parameters for a channel.
         *
         * @param channelId The channel ID.
         * @return Map of effect parameter names to their values.
         */
        std::unordered_map<AmString, AmReal32> CollectChannelEffectParameters(AmChannelID channelId) const;

        // Member variables
        bool _initialized;
        Engine* _engine; // Reference to the engine instance

        // Performance monitoring state
        mutable AmUInt64 _lastMemoryCheck;
        mutable AmReal32 _lastCpuCheck;
        mutable std::chrono::high_resolution_clock::time_point _lastPerformanceUpdate;

        // Cached performance data to avoid frequent system calls
        mutable AmUInt64 _cachedMemoryUsage;
        mutable AmReal32 _cachedCpuUsage;
        mutable std::chrono::high_resolution_clock::time_point _lastCacheUpdate;
        static constexpr AmReal32 kPerformanceCacheLifetimeSeconds = 0.1f; // Cache for 100ms
    };

} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_DATA_COLLECTOR_H
