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

#ifndef _AM_PROFILER_DATA_H
#define _AM_PROFILER_DATA_H

#include <SparkyStudios/Audio/Amplitude/Core/Entity.h>
#include <SparkyStudios/Audio/Amplitude/Core/Event.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Types.h>

namespace SparkyStudios::Audio::Amplitude
{
    /**
     * @brief Base class for all profiler data snapshots.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerDataSnapshot
    {
        ProfilerTime mTimestamp;
        ProfilerMessageID mMessageId;
        eProfilerCategory mCategory;
        eProfilerPriority mPriority;

        ProfilerDataSnapshot();

        virtual ~ProfilerDataSnapshot() = default;

    private:
        static std::atomic<ProfilerMessageID> _sMessageIdCounter;
        static ProfilerMessageID GenerateMessageId();
    };

    /**
     * @brief Engine state snapshot.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerEngineData : public ProfilerDataSnapshot
    {
        // Engine state
        bool mIsInitialized;
        AmReal64 mEngineUptime;
        AmString mConfigFile;

        // Counts
        AmUInt32 mTotalEntityCount;
        AmUInt32 mActiveEntityCount;

        AmUInt32 mTotalChannelCount;
        AmUInt32 mActiveChannelCount;

        AmUInt32 mTotalListenerCount;
        AmUInt32 mActiveListenerCount;

        AmUInt32 mTotalEnvironmentCount;
        AmUInt32 mActiveEnvironmentCount;

        AmUInt32 mTotalRoomCount;
        AmUInt32 mActiveRoomCount;

        // Performance metrics
        AmReal32 mCpuUsagePercent;
        AmUInt64 mMemoryUsageBytes;
        AmUInt64 mMemoryPeakBytes;
        AmUInt32 mActiveVoiceCount;
        AmUInt32 mMaxVoiceCount;

        // Audio system state
        AmUInt32 mSampleRate;
        AmUInt16 mChannelCount;
        AmUInt16 mFrameCount;
        AmReal32 mMasterGain;

        // Loaded assets
        std::vector<AmString> mLoadedSoundBanks;
        std::vector<AmString> mLoadedPlugins;
        std::unordered_map<AmString, AmUInt32> mAssetCounts;

        ProfilerEngineData();
    };

    /**
     * @brief Entity state snapshot.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerEntityData : public ProfilerDataSnapshot
    {
        AmEntityID mEntityId;
        AmVector3 mPosition;
        AmVector3 mLastPosition;
        AmVector3 mVelocity;
        AmVector3 mForward;
        AmVector3 mUp;

        // Entity-specific audio state
        AmUInt32 mActiveChannelCount;
        AmReal32 mDistanceToListener;
        AmReal32 mObstruction;
        AmReal32 mOcclusion;
        AmReal32 mDirectivity;
        AmReal32 mDirectivitySharpness;

        // Spatialization info
        AmReal32 mAzimuth;
        AmReal32 mElevation;
        AmReal32 mAttenuationFactor;

        // Associated channels
        std::vector<AmChannelID> mChannelIds;

        // Environment effects
        std::map<AmEnvironmentID, AmReal32> mEnvironmentEffects;

        ProfilerEntityData();
    };

    /**
     * @brief Channel state snapshot.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerChannelData : public ProfilerDataSnapshot
    {
        AmChannelID mChannelId;
        eChannelPlaybackState mPlaybackState;
        AmEntityID mSourceEntityId;

        // Playback information
        AmString mSoundName;
        AmString mSoundBankName;
        AmString mCollectionName;
        AmTime mPlaybackPosition;
        AmTime mTotalDuration;
        AmUInt32 mLoopCount;
        AmUInt32 mCurrentLoop;

        // Audio parameters
        AmReal32 mGain;

        // 3D audio state
        AmVector3 mPosition;
        AmReal32 mDistanceToListener;
        AmReal32 mDopplerFactor;
        AmReal32 mOcclusionFactor;
        AmReal32 mObstructionFactor;

        // Effects chain
        std::vector<AmString> mActiveEffects;
        std::unordered_map<AmString, AmReal32> mEffectParameters;

        ProfilerChannelData();
    };

    /**
     * @brief Listener state snapshot.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerListenerData : public ProfilerDataSnapshot
    {
        AmListenerID mListenerId;
        AmVector3 mPosition;
        AmVector3 mLastPosition;
        AmVector3 mVelocity;
        AmVector3 mForward;
        AmVector3 mUp;
        AmReal32 mGain;

        // Environment
        AmString mCurrentEnvironment;
        std::unordered_map<AmString, AmReal32> mEnvironmentParameters;

        ProfilerListenerData();
    };

    /**
     * @brief Performance metrics snapshot.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerPerformanceData : public ProfilerDataSnapshot
    {
        // CPU metrics
        AmReal32 mTotalCpuUsage;
        AmReal32 mMixerCpuUsage;
        AmReal32 mDspCpuUsage;
        AmReal32 mStreamingCpuUsage;

        // Memory metrics
        AmUInt64 mTotalAllocatedMemory;
        AmUInt64 mEngineMemory;
        AmUInt64 mAudioBufferMemory;
        AmUInt64 mAssetMemory;

        // Audio pipeline metrics
        AmUInt32 mProcessedSamples;
        AmUInt32 mUnderruns;
        AmUInt32 mOverruns;
        AmReal32 mLatencyMs;

        // Threading info
        AmUInt32 mActiveThreadCount;
        std::unordered_map<AmString, AmReal32> mThreadCpuUsage;

        ProfilerPerformanceData();
    };

    /**
     * @brief Generic profiler event.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerEvent : public ProfilerDataSnapshot
    {
        AmString mEventName;
        AmString mDescription;
        std::unordered_map<AmString, AmString> mParameters;

        ProfilerEvent()
        {
            mCategory = eProfilerCategory_Events;
        }

        ProfilerEvent(const AmString& name, const AmString& desc = "")
            : mEventName(name)
            , mDescription(desc)
        {
            mCategory = eProfilerCategory_Events;
        }
    };

    /**
     * @brief Variant type that can hold any profiler data
     */
    using ProfilerDataVariant = std::
        variant<ProfilerEngineData, ProfilerEntityData, ProfilerChannelData, ProfilerListenerData, ProfilerPerformanceData, ProfilerEvent>;
} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_DATA_H
