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

#ifndef _AM_PROFILER_MANAGER_H
#define _AM_PROFILER_MANAGER_H

#include <atomic>
#include <chrono>
#include <memory>

#include <SparkyStudios/Audio/Amplitude/Core/Thread.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Config.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/DataCollector.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Messaging.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Server.h>

namespace SparkyStudios::Audio::Amplitude
{
    // Forward declarations
    class ProfilerDataCollector;

    /**
     * @brief Central manager for the profiling system.
     *
     * This singleton manages all aspects of the profiling system including
     * data collection, filtering, and distribution to connected clients.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerManager
    {
    public:
        /**
         * @brief Get the singleton instance.
         */
        static ProfilerManager* GetInstance();

        /**
         * @brief Destroy the singleton instance.
         */
        static void DestroyInstance();

        // Private constructor/destructor for singleton
        ProfilerManager();
        ~ProfilerManager();

        // Non-copyable, non-movable
        ProfilerManager(const ProfilerManager&) = delete;
        ProfilerManager& operator=(const ProfilerManager&) = delete;
        ProfilerManager(ProfilerManager&&) = delete;
        ProfilerManager& operator=(ProfilerManager&&) = delete;

        // Lifecycle management
        bool Initialize(const ProfilerConfig& config);
        bool Initialize(const AmOsString& configFile);
        void Deinitialize();
        AM_INLINE bool IsInitialized() const
        {
            return _initialized.load();
        }
        AM_INLINE bool IsEnabled() const
        {
            return _enabled.load();
        }

        // Configuration
        AM_INLINE const ProfilerConfig& GetConfig() const
        {
            return _config;
        }
        bool UpdateConfig(const ProfilerConfig& newConfig);

        // Data capture control
        void SetEnabled(bool enabled);
        void SetCategoryMask(AmUInt32 categoryMask);
        void SetUpdateMode(eProfilerUpdateMode mode);
        void SetUpdateFrequency(AmReal32 frequencyHz);

        // Manual data capture
        void CaptureEngineState();
        void CaptureEntityState(AmEntityID entityId);
        void CaptureChannelState(AmChannelID channelId);
        void CaptureListenerState(AmListenerID listenerId);
        void CapturePerformanceMetrics();
        void CaptureEvent(const ProfilerEvent& event);

        // Bulk capture operations
        void CaptureAllEntities();
        void CaptureAllChannels();
        void CaptureAllListeners();
        void CaptureFullState();

        // Network management
        bool StartNetworkServer();
        void StopNetworkServer();
        bool IsNetworkServerRunning() const;
        AmUInt32 GetConnectedClientCount() const;

        // Statistics
        struct Statistics
        {
            AmUInt64 totalMessagesSent;
            AmUInt64 messagesDropped;
            AmUInt64 bytesTransmitted;
            AmReal32 averageMessageSizeBytes;
            AmReal32 currentUpdateRate;
            AmUInt32 activeClients;
        };

        Statistics GetStatistics() const;
        void ResetStatistics();

        // Callback registration for local consumption
        using MessageCallback = std::function<void(const ProfilerDataVariant&)>;
        void RegisterMessageCallback(const MessageCallback& callback);
        void UnregisterMessageCallback();

    private:
        // Core update loop
        void UpdateLoop();
        void ProcessQueuedMessages();

        // Data collection helpers
        void CollectTimedUpdates();
        void CollectOnChangeUpdates();
        bool ShouldCaptureCategory(eProfilerCategory category) const;
        bool HasSignificantChange(const ProfilerDataVariant& newData, const ProfilerDataVariant& oldData) const;

        // Message processing
        void QueueMessage(ProfilerDataVariant&& message);
        void DistributeMessage(const ProfilerDataVariant& message);

        // Threading
        void StartUpdateThread();
        void StopUpdateThread();

        // Member variables
        static AmUniquePtr<ProfilerManager, eMemoryPoolKind_Engine> _sInstance;
        static AmMutexHandle _sInstanceMutex;

        std::atomic<bool> _initialized;
        std::atomic<bool> _enabled;
        std::atomic<bool> _running;

        ProfilerConfig _config;

        // Threading
        AmThreadHandle _updateThread;
        AmMutexHandle _configMutex;

        // Data management
        AmUniquePtr<ProfilerDataCollector, eMemoryPoolKind_IO> _dataCollector;
        AmUniquePtr<ProfilerMessageQueue, eMemoryPoolKind_IO> _messageQueue;
        AmUniquePtr<ProfilerMessagePool, eMemoryPoolKind_IO> _messagePool;

        // Network
        AmUniquePtr<ProfilerServer, eMemoryPoolKind_IO> _networkServer;

        // Statistics
        mutable AmMutexHandle _statisticsMutex;
        Statistics _statistics;

        // Last known states for change detection
        std::unordered_map<AmEntityID, ProfilerEntityData> _lastEntityStates;
        std::unordered_map<AmChannelID, ProfilerChannelData> _lastChannelStates;
        std::unordered_map<AmListenerID, ProfilerListenerData> _lastListenerStates;
        ProfilerEngineData _lastEngineState;

        // Timing
        std::chrono::high_resolution_clock::time_point _lastUpdate;
        AmReal32 _updateInterval;

        // Local callback
        MessageCallback _localCallback;
        AmMutexHandle _callbackMutex;
    };

// Convenience macros
#if defined(AM_PROFILER_ENABLED)
#define amProfiler ProfilerManager::GetInstance()
#define AM_PROFILER_CAPTURE_ENGINE()                                                                                                       \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (amProfiler->IsEnabled())                                                                                                       \
            amProfiler->CaptureEngineState();                                                                                              \
    } while (0)
#define AM_PROFILER_CAPTURE_ENTITY(id)                                                                                                     \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (amProfiler->IsEnabled())                                                                                                       \
            amProfiler->CaptureEntityState(id);                                                                                            \
    } while (0)
#define AM_PROFILER_CAPTURE_CHANNEL(id)                                                                                                    \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (amProfiler->IsEnabled())                                                                                                       \
            amProfiler->CaptureChannelState(id);                                                                                           \
    } while (0)
#define AM_PROFILER_EVENT(name, desc)                                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (amProfiler->IsEnabled())                                                                                                       \
            amProfiler->CaptureEvent(ProfilerEvent(name, desc));                                                                           \
    } while (0)
#else
#define amProfiler (static_cast<ProfilerManager*>(nullptr))
#define AM_PROFILER_CAPTURE_ENGINE()                                                                                                       \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (0)
#define AM_PROFILER_CAPTURE_ENTITY(id)                                                                                                     \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (0)
#define AM_PROFILER_CAPTURE_CHANNEL(id)                                                                                                    \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (0)
#define AM_PROFILER_EVENT(name, desc)                                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (0)
#endif
} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_MANAGER_H
