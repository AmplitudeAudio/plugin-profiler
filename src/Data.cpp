// Copyright (c) 2025-present Sparky Studios. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>

namespace SparkyStudios::Audio::Amplitude
{
    std::atomic<ProfilerMessageID> ProfilerDataSnapshot::_sMessageIdCounter{ 0 };

    ProfilerDataSnapshot::ProfilerDataSnapshot()
        : mTimestamp(std::chrono::high_resolution_clock::now())
        , mMessageId(GenerateMessageId())
        , mCategory(eProfilerCategory_Engine)
        , mPriority(eProfilerPriority_Normal)
    {}

    ProfilerMessageID ProfilerDataSnapshot::GenerateMessageId()
    {
        return ++_sMessageIdCounter;
    }

    ProfilerEngineData::ProfilerEngineData()
    {
        mCategory = eProfilerCategory_Engine;
        mIsInitialized = false;
        mEngineUptime = 0.0;
        mTotalEntityCount = mActiveEntityCount = 0;
        mTotalChannelCount = mActiveChannelCount = 0;
        mTotalListenerCount = mActiveListenerCount = 0;
        mCpuUsagePercent = 0.0f;
        mMemoryUsageBytes = mMemoryPeakBytes = 0;
        mActiveVoiceCount = mMaxVoiceCount = 0;
        mSampleRate = 0;
        mChannelCount = mFrameCount = 0;
        mMasterGain = 1.0f;
    }

    ProfilerEntityData::ProfilerEntityData()
        : mChannelIds()
        , mEnvironmentEffects()
    {
        mCategory = eProfilerCategory_Entity;
        mEntityId = kAmInvalidObjectId;
        mPosition = mLastPosition = mVelocity = kVector3Zero;
        mForward = mUp = kVector3Zero;

        mActiveChannelCount = 0;
        mDistanceToListener = 0.0f;
        mObstruction = mOcclusion = 0.0f;
        mDirectivity = mDirectivitySharpness = 0.0f;

        mAzimuth = mElevation = 0.0f;
        mAttenuationFactor = 1.0f;
    }

    ProfilerChannelData::ProfilerChannelData()
    {
        mCategory = eProfilerCategory_Channel;
        mChannelId = kAmInvalidObjectId;
        mPlaybackState = eChannelPlaybackState_Stopped;
        mSourceEntityId = kAmInvalidObjectId;
        mPlaybackPosition = mTotalDuration = 0;
        mLoopCount = mCurrentLoop = 0;
        mGain = 1.0f;
        mDistanceToListener = 0.0f;
        mDopplerFactor = mOcclusionFactor = mObstructionFactor = 1.0f;
    }

    ProfilerListenerData::ProfilerListenerData()
        : mEnvironmentParameters()
    {
        mCategory = eProfilerCategory_Listener;
        mListenerId = kAmInvalidObjectId;
        mPosition = mLastPosition = mVelocity = kVector3Zero;
        mForward = mUp = kVector3Zero;
        mGain = 1.0f;
    }

    ProfilerPerformanceData::ProfilerPerformanceData()
    {
        mCategory = eProfilerCategory_Performance;
        mTotalCpuUsage = mMixerCpuUsage = mDspCpuUsage = mStreamingCpuUsage = 0.0f;
        mTotalAllocatedMemory = mEngineMemory = mAudioBufferMemory = mAssetMemory = 0;
        mProcessedSamples = mUnderruns = mOverruns = 0;
        mLatencyMs = 0.0f;
        mActiveThreadCount = 0;
    }

} // namespace SparkyStudios::Audio::Amplitude
