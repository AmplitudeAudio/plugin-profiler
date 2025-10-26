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

#include <SparkyStudios/Audio/Amplitude/Core/Engine.h>
#include <SparkyStudios/Audio/Amplitude/IO/Log.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/DataCollector.h>

#include <Plugin.h>

namespace SparkyStudios::Audio::Amplitude
{
    ProfilerDataCollector::ProfilerDataCollector()
        : _initialized(false)
        , _lastMemoryCheck(0)
        , _lastCpuCheck(0.0f)
        , _lastPerformanceUpdate(std::chrono::high_resolution_clock::now())
        , _cachedMemoryUsage(0)
        , _cachedCpuUsage(0.0f)
        , _lastCacheUpdate(std::chrono::high_resolution_clock::now())
    {
        amLogDebug("[ProfilerDataCollector] Created data collector");
    }

    ProfilerDataCollector::~ProfilerDataCollector()
    {
        Deinitialize();
        amLogDebug("[ProfilerDataCollector] Destroyed data collector");
    }

    bool ProfilerDataCollector::Initialize()
    {
        if (_initialized)
        {
            amLogWarning("[ProfilerDataCollector] Already initialized");
            return true;
        }

        if (!amEngine)
        {
            amLogError("[ProfilerDataCollector] Engine instance not available");
            return false;
        }

        if (!amEngine->IsInitialized())
        {
            amLogError("[ProfilerDataCollector] Engine not initialized");
            return false;
        }

        _initialized = true;
        amLogInfo("[ProfilerDataCollector] Data collector initialized successfully");
        return true;
    }

    void ProfilerDataCollector::Deinitialize()
    {
        if (!_initialized)
            return;

        amEngine = nullptr;
        _initialized = false;
        amLogInfo("[ProfilerDataCollector] Data collector deinitialized");
    }

    bool ProfilerDataCollector::IsInitialized() const
    {
        return _initialized;
    }

    ProfilerEngineData ProfilerDataCollector::CollectEngineData() const
    {
        ProfilerEngineData data;

        if (!amEngine)
        {
            amLogWarning("[ProfilerDataCollector] Engine not available for engine data collection");
            return data;
        }

        // Basic engine state
        data.mIsInitialized = amEngine->IsInitialized();
        data.mEngineUptime = amEngine->GetTotalTime();
        data.mConfigFile = amEngine->GetConfigurationPath();

        // Counts
        data.mTotalEntityCount = amEngine->GetMaxEntitiesCount();
        data.mActiveEntityCount = amEngine->GetActiveEntitiesCount(); // TODO: Get from engine
        data.mTotalChannelCount = 0; // TODO: Get from engine
        data.mActiveChannelCount = 0; // TODO: Get from engine
        data.mTotalListenerCount = amEngine->GetMaxListenersCount();
        data.mActiveListenerCount = amEngine->GetActiveListenersCount(); // TODO: Get from engine
        data.mTotalEnvironmentCount = amEngine->GetMaxEnvironmentsCount();
        data.mActiveEnvironmentCount = amEngine->GetActiveEnvironmentsCount(); // TODO: Get from engine
        data.mTotalRoomCount = amEngine->GetMaxRoomsCount();
        data.mActiveRoomCount = amEngine->GetActiveRoomsCount(); // TODO: Get from engine

        // Performance metrics
        data.mCpuUsagePercent = GetCurrentCpuUsage();
        data.mMemoryUsageBytes = GetCurrentMemoryUsage();
        data.mMemoryPeakBytes = GetPeakMemoryUsage();
        data.mActiveVoiceCount = GetActiveVoiceCount();
        data.mMaxVoiceCount = GetMaxVoiceCount();

        const auto device = amEngine->GetMixer()->GetDeviceDescription();

        // Audio system state
        data.mSampleRate = device.mDeviceOutputSampleRate;
        data.mChannelCount = static_cast<AmInt16>(device.mDeviceOutputChannels);
        data.mFrameCount = device.mOutputBufferSize;
        data.mMasterGain = amEngine->GetMasterGain();

        // Loaded assets
        data.mLoadedSoundBanks = GetLoadedSoundBanks();
        data.mLoadedPlugins = GetLoadedPlugins();
        data.mAssetCounts = GetAssetCounts();

        return data;
    }

    ProfilerEntityData ProfilerDataCollector::CollectEntityData(AmEntityID entityId) const
    {
        ProfilerEntityData data;
        data.mEntityId = entityId;

        if (!amEngine)
        {
            amLogWarning("[ProfilerDataCollector] Engine not available for entity data collection");
            return data;
        }

        const auto entity = amEngine->GetEntity(entityId);
        if (!entity.Valid())
        {
            amLogWarning("[ProfilerDataCollector] Entity not found for data collection");
            return data;
        }

        data.mPosition = entity.GetLocation();
        data.mVelocity = entity.GetVelocity();
        data.mForward = entity.GetDirection();
        data.mUp = entity.GetUp();

        data.mObstruction = entity.GetObstruction();
        data.mOcclusion = entity.GetOcclusion();
        data.mDirectivity = entity.GetDirectivity();
        data.mDirectivitySharpness = entity.GetDirectivitySharpness();

        data.mActiveChannelCount = entity.GetActiveChannelCount();
        data.mDistanceToListener = CalculateDistanceToListener(entityId);

        data.mAttenuationFactor = CalculateAttenuationFactor(entityId);
        CalculateSphericalPosition(entityId, data.mAzimuth, data.mElevation);

        data.mEnvironmentEffects = entity.GetEnvironments();

        return data;
    }

    ProfilerChannelData ProfilerDataCollector::CollectChannelData(AmChannelID channelId) const
    {
        ProfilerChannelData data;
        data.mChannelId = channelId;

        if (!amEngine)
        {
            amLogWarning("[ProfilerDataCollector] Engine not available for channel data collection");
            return data;
        }

        const Channel channel = amEngine->GetChannel(channelId);
        if (!channel.Valid())
        {
            amLogWarning("[ProfilerDataCollector] Channel not found for data collection");
            return data;
        }

        data.mPlaybackState = channel.GetPlaybackState();
        data.mSourceEntityId = channel.GetEntity().GetId();

        data.mSoundName = "unknown_sound";
        data.mSoundBankName = "unknown_bank";
        data.mCollectionName = "";

        data.mPlaybackPosition = 0;
        data.mTotalDuration = 0;
        data.mLoopCount = 0;
        data.mCurrentLoop = 0;

        data.mGain = channel.GetGain();

        data.mPosition = channel.GetLocation();
        data.mDistanceToListener = 0; // TODO: Length(Sub(channel.GetListener().GetLocation(), channel.GetLocation()));
        data.mDopplerFactor = 0;
        data.mOcclusionFactor = 1.0f;
        data.mObstructionFactor = 1.0f;

        data.mActiveEffects = CollectChannelEffects(channelId);
        data.mEffectParameters = CollectChannelEffectParameters(channelId);

        return data;
    }

    ProfilerListenerData ProfilerDataCollector::CollectListenerData(AmListenerID listenerId) const
    {
        ProfilerListenerData data;
        data.mListenerId = listenerId;

        if (!amEngine)
            return data;

        const auto listener = amEngine->GetListener(listenerId);
        if (!listener.Valid())
        {
            amLogWarning("[ProfilerDataCollector] Listener not found for data collection");
            return data;
        }

        data.mLastPosition = data.mPosition;
        data.mPosition = listener.GetLocation();
        data.mVelocity = listener.GetVelocity();
        data.mForward = listener.GetDirection();
        data.mUp = listener.GetUp();
        data.mGain = 1.0f;

        data.mCurrentEnvironment = "default";
        // data.mEnvironmentParameters would be populated with actual environment data

        return data;
    }

    ProfilerPerformanceData ProfilerDataCollector::CollectPerformanceData() const
    {
        ProfilerPerformanceData data;

        // CPU metrics
        data.mTotalCpuUsage = GetCurrentCpuUsage();
        data.mMixerCpuUsage = data.mTotalCpuUsage * 0.4f; // Estimated breakdown
        data.mDspCpuUsage = data.mTotalCpuUsage * 0.3f;
        data.mStreamingCpuUsage = data.mTotalCpuUsage * 0.1f;

        // Memory metrics
        data.mTotalAllocatedMemory = GetCurrentMemoryUsage();
        data.mEngineMemory = data.mTotalAllocatedMemory * 0.3f; // Estimated breakdown
        data.mAudioBufferMemory = data.mTotalAllocatedMemory * 0.5f;
        data.mAssetMemory = data.mTotalAllocatedMemory * 0.2f;

        // Audio pipeline metrics - would need engine API
        data.mProcessedSamples = 0; // TODO: Get from engine
        data.mUnderruns = 0; // TODO: Get from engine
        data.mOverruns = 0; // TODO: Get from engine
        data.mLatencyMs = 10.0f; // TODO: Get from engine

        // Threading info
        data.mActiveThreadCount = 1; // TODO: Get actual thread count
        data.mThreadCpuUsage["main"] = data.mTotalCpuUsage * 0.6f;
        data.mThreadCpuUsage["audio"] = data.mTotalCpuUsage * 0.4f;

        return data;
    }

    std::vector<AmEntityID> ProfilerDataCollector::GetAllEntityIds() const
    {
        std::vector<AmEntityID> entityIds;

        if (!amEngine)
            return entityIds;

        // TODO: Implement actual entity enumeration from engine
        // For now, return empty list

        return entityIds;
    }

    std::vector<AmChannelID> ProfilerDataCollector::GetAllChannelIds() const
    {
        std::vector<AmChannelID> channelIds;

        if (!amEngine)
        {
            return channelIds;
        }

        // TODO: Implement actual channel enumeration from engine
        // For now, return empty list

        return channelIds;
    }

    std::vector<AmListenerID> ProfilerDataCollector::GetAllListenerIds() const
    {
        std::vector<AmListenerID> listenerIds;

        if (!amEngine)
        {
            return listenerIds;
        }

        // TODO: Implement actual listener enumeration from engine
        // For now, return empty list

        return listenerIds;
    }

    std::vector<ProfilerEntityData> ProfilerDataCollector::CollectAllEntityData() const
    {
        std::vector<ProfilerEntityData> entityData;

        auto entityIds = GetAllEntityIds();
        entityData.reserve(entityIds.size());

        for (AmEntityID entityId : entityIds)
        {
            entityData.push_back(CollectEntityData(entityId));
        }

        return entityData;
    }

    std::vector<ProfilerChannelData> ProfilerDataCollector::CollectAllChannelData() const
    {
        std::vector<ProfilerChannelData> channelData;

        auto channelIds = GetAllChannelIds();
        channelData.reserve(channelIds.size());

        for (AmChannelID channelId : channelIds)
        {
            channelData.push_back(CollectChannelData(channelId));
        }

        return channelData;
    }

    std::vector<ProfilerListenerData> ProfilerDataCollector::CollectAllListenerData() const
    {
        std::vector<ProfilerListenerData> listenerData;

        auto listenerIds = GetAllListenerIds();
        listenerData.reserve(listenerIds.size());

        for (AmListenerID listenerId : listenerIds)
        {
            listenerData.push_back(CollectListenerData(listenerId));
        }

        return listenerData;
    }

    AmUInt64 ProfilerDataCollector::GetCurrentMemoryUsage() const
    {
        // Check cache first
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<AmReal32>(now - _lastCacheUpdate).count();

        if (elapsed < kPerformanceCacheLifetimeSeconds && _cachedMemoryUsage > 0)
        {
            return _cachedMemoryUsage;
        }

        AmUInt64 memoryUsage = 0;

#if AM_PLATFORM_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            memoryUsage = pmc.WorkingSetSize;
        }
#elif AM_PLATFORM_LINUX || AM_PLATFORM_APPLE
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
#if AM_PLATFORM_LINUX
            memoryUsage = usage.ru_maxrss * 1024; // Linux reports in KB
#else
            memoryUsage = usage.ru_maxrss; // macOS reports in bytes
#endif
        }
#endif

        // Update cache
        _cachedMemoryUsage = memoryUsage;
        _lastCacheUpdate = now;

        return memoryUsage;
    }

    AmUInt64 ProfilerDataCollector::GetPeakMemoryUsage() const
    {
        // TODO: Implement peak memory tracking
        // For now, return current usage
        return GetCurrentMemoryUsage();
    }

    AmReal32 ProfilerDataCollector::GetCurrentCpuUsage() const
    {
        // Check cache first
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<AmReal32>(now - _lastCacheUpdate).count();

        if (elapsed < kPerformanceCacheLifetimeSeconds && _cachedCpuUsage > 0.0f)
        {
            return _cachedCpuUsage;
        }

        AmReal32 cpuUsage = 0.0f;

        // TODO: Implement actual CPU usage measurement
        // This is platform-specific and complex to implement properly
        // For now, return a reasonable stub value
        cpuUsage = 5.0f; // Assume 5% CPU usage

        // Update cache
        _cachedCpuUsage = cpuUsage;
        _lastCacheUpdate = now;

        return cpuUsage;
    }

    AmUInt32 ProfilerDataCollector::GetActiveVoiceCount() const
    {
        if (!amEngine)
        {
            return 0;
        }

        // TODO: Get actual voice count from engine
        return 0;
    }

    AmUInt32 ProfilerDataCollector::GetMaxVoiceCount() const
    {
        if (!amEngine)
        {
            return 0;
        }

        // TODO: Get max voice count from engine configuration
        return 64; // Reasonable default
    }

    std::vector<AmString> ProfilerDataCollector::GetLoadedPlugins() const
    {
        std::vector<AmString> plugins;

        if (!amEngine)
        {
            return plugins;
        }

        // TODO: Implement actual plugin enumeration from engine
        // For now, return common plugin types
        plugins.push_back("codec_wav");
        plugins.push_back("codec_ogg");

        return plugins;
    }

    std::vector<AmString> ProfilerDataCollector::GetLoadedSoundBanks() const
    {
        std::vector<AmString> soundBanks;

        if (!amEngine)
        {
            return soundBanks;
        }

        // TODO: Implement actual sound bank enumeration from engine
        // For now, return empty list

        return soundBanks;
    }

    std::unordered_map<AmString, AmUInt32> ProfilerDataCollector::GetAssetCounts() const
    {
        std::unordered_map<AmString, AmUInt32> counts;

        if (!amEngine)
        {
            return counts;
        }

        // TODO: Implement actual asset counting from engine
        // For now, return stub data
        counts["sounds"] = 0;
        counts["collections"] = 0;
        counts["switch_containers"] = 0;
        counts["effects"] = 0;
        counts["attenuation_models"] = 0;

        return counts;
    }

    AmReal32 ProfilerDataCollector::CalculateDistanceToListener(AmEntityID entityId, AmListenerID listenerId) const
    {
        // TODO: Implement actual distance calculation
        // Would need to get entity and listener positions from engine
        return 0.0f;
    }

    AmReal32 ProfilerDataCollector::CalculateAttenuationFactor(AmEntityID entityId) const
    {
        // TODO: Implement actual attenuation calculation
        // Would need to apply attenuation model based on distance
        AmReal32 distance = CalculateDistanceToListener(entityId);

        // Simple linear attenuation for stub
        if (distance > 1000.0f)
            return 0.0f;

        return 1.0f - (distance / 1000.0f);
    }

    void ProfilerDataCollector::CalculateSphericalPosition(AmEntityID entityId, AmReal32& azimuth, AmReal32& elevation) const
    {
        // TODO: Implement actual spherical coordinate calculation
        // Would need entity and listener positions and orientations
        azimuth = 0.0f;
        elevation = 0.0f;
    }

    std::vector<AmString> ProfilerDataCollector::CollectChannelEffects(AmChannelID channelId) const
    {
        std::vector<AmString> effects;

        // TODO: Implement actual channel effect collection
        // For now, return empty list

        return effects;
    }

    std::unordered_map<AmString, AmReal32> ProfilerDataCollector::CollectChannelEffectParameters(AmChannelID channelId) const
    {
        std::unordered_map<AmString, AmReal32> parameters;

        // TODO: Implement actual channel effect parameter collection
        // For now, return empty map

        return parameters;
    }

} // namespace SparkyStudios::Audio::Amplitude
