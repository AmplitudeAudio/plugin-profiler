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
#include <SparkyStudios/Audio/Amplitude/Profiler/Manager.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Server.h>

namespace SparkyStudios::Audio::Amplitude
{
    // Static member definitions
    AmUniquePtr<ProfilerManager, eMemoryPoolKind_Engine> ProfilerManager::_sInstance = nullptr;
    AmMutexHandle ProfilerManager::_sInstanceMutex = Thread::CreateMutex();

    ProfilerManager* ProfilerManager::GetInstance()
    {
        Thread::LockMutex(_sInstanceMutex);
        if (!_sInstance)
            _sInstance = AmUniquePtr<ProfilerManager, eMemoryPoolKind_Engine>(ampoolnew(eMemoryPoolKind_Engine, ProfilerManager));
        Thread::UnlockMutex(_sInstanceMutex);
        return _sInstance.get();
    }

    void ProfilerManager::DestroyInstance()
    {
        Thread::LockMutex(_sInstanceMutex);
        if (_sInstance)
        {
            _sInstance->Deinitialize();
            _sInstance.reset();
        }
        Thread::UnlockMutex(_sInstanceMutex);
    }

    ProfilerManager::ProfilerManager()
        : _initialized(false)
        , _enabled(false)
        , _running(false)
        , _updateThread(nullptr)
        , _updateInterval(1.0f / 30.0f) // 30 FPS default
        , _lastUpdate(std::chrono::high_resolution_clock::now())
    {
        _configMutex = Thread::CreateMutex();
        _statisticsMutex = Thread::CreateMutex();
        _callbackMutex = Thread::CreateMutex();

        _messageQueue = std::make_unique<ProfilerMessageQueue>();
        _messagePool = std::make_unique<ProfilerMessagePool>();

        // Initialize statistics
        Thread::LockMutex(_statisticsMutex);
        _statistics = {};
        Thread::UnlockMutex(_statisticsMutex);
    }

    ProfilerManager::~ProfilerManager()
    {
        Deinitialize();

        if (_configMutex)
            Thread::DestroyMutex(_configMutex);
        if (_statisticsMutex)
            Thread::DestroyMutex(_statisticsMutex);
        if (_callbackMutex)
            Thread::DestroyMutex(_callbackMutex);
    }

    bool ProfilerManager::Initialize(const ProfilerConfig& config)
    {
        if (_initialized.load())
        {
            amLogWarning("[ProfilerManager] Already initialized");
            return true;
        }

        {
            Thread::LockMutex(_configMutex);
            _config = config;

            // Validate configuration
            if (!_config.Validate())
            {
                amLogError("[ProfilerManager] Invalid profiler configuration");
                Thread::UnlockMutex(_configMutex);
                return false;
            }

            _updateInterval = 1.0f / _config.mUpdateFrequencyHz;
            Thread::UnlockMutex(_configMutex);
        }

        // Initialize data collector
        _dataCollector = std::make_unique<ProfilerDataCollector>();

        // Start network server if enabled
        if (_config.mEnableNetworking)
        {
            if (!StartNetworkServer())
            {
                amLogError("[ProfilerManager] Failed to start network server");
                return false;
            }
        }

        // Start update thread
        StartUpdateThread();

        _initialized = true;
        _enabled = true;

        amLogInfo("[ProfilerManager] Profiler system initialized successfully");
        return true;
    }

    bool ProfilerManager::Initialize(const AmOsString& configFile)
    {
        ProfilerConfig config;
        if (!config.LoadFromFile(configFile))
        {
            amLogError("[ProfilerManager] Failed to load profiler config from file");
            return false;
        }

        return Initialize(config);
    }

    void ProfilerManager::Deinitialize()
    {
        if (!_initialized.load())
            return;

        _enabled = false;

        // Stop update thread
        StopUpdateThread();

        // Stop network server
        StopNetworkServer();

        // Clear queued messages
        _messageQueue->Clear();

        // Clean up
        _dataCollector.reset();
        _networkServer.reset();
        _messagePool.reset();
        _messageQueue.reset();

        // Clear state caches
        _lastEntityStates.clear();
        _lastChannelStates.clear();
        _lastListenerStates.clear();

        _initialized = false;

        amLogInfo("[ProfilerManager] Profiler system deinitialized");
    }

    bool ProfilerManager::UpdateConfig(const ProfilerConfig& newConfig)
    {
        if (!newConfig.Validate())
        {
            amLogError("[ProfilerManager] Invalid configuration provided");
            return false;
        }

        Thread::LockMutex(_configMutex);
        ProfilerConfig oldConfig = _config;
        _config = newConfig;
        _updateInterval = 1.0f / _config.mUpdateFrequencyHz;
        Thread::UnlockMutex(_configMutex);

        // Restart network server if network settings changed
        if (oldConfig.mEnableNetworking != newConfig.mEnableNetworking || oldConfig.mServerPort != newConfig.mServerPort ||
            oldConfig.mBindAddress != newConfig.mBindAddress)
        {
            StopNetworkServer();
            if (_config.mEnableNetworking)
            {
                StartNetworkServer();
            }
        }

        amLogInfo("[ProfilerManager] Configuration updated successfully");
        return true;
    }

    void ProfilerManager::SetEnabled(bool enabled)
    {
        _enabled = enabled;
        amLogInfo("[ProfilerManager] Profiler %s", enabled ? "enabled" : "disabled");
    }

    void ProfilerManager::SetCategoryMask(AmUInt32 categoryMask)
    {
        Thread::LockMutex(_configMutex);
        _config.mCategoryMask = categoryMask;
        Thread::UnlockMutex(_configMutex);
    }

    void ProfilerManager::SetUpdateMode(eProfilerUpdateMode mode)
    {
        Thread::LockMutex(_configMutex);
        _config.mUpdateMode = mode;
        Thread::UnlockMutex(_configMutex);
    }

    void ProfilerManager::SetUpdateFrequency(AmReal32 frequencyHz)
    {
        Thread::LockMutex(_configMutex);
        _config.mUpdateFrequencyHz = frequencyHz;
        _updateInterval = 1.0f / frequencyHz;
        Thread::UnlockMutex(_configMutex);
    }

    void ProfilerManager::CaptureEngineState()
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Engine))
            return;

        if (_dataCollector)
        {
            ProfilerEngineData data = _dataCollector->CollectEngineData();
            QueueMessage(std::move(data));
        }
    }

    void ProfilerManager::CaptureEntityState(AmEntityID entityId)
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Entity))
            return;

        if (_dataCollector)
        {
            ProfilerEntityData data = _dataCollector->CollectEntityData(entityId);
            QueueMessage(std::move(data));
        }
    }

    void ProfilerManager::CaptureChannelState(AmChannelID channelId)
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Channel))
            return;

        if (_dataCollector)
        {
            ProfilerChannelData data = _dataCollector->CollectChannelData(channelId);
            QueueMessage(std::move(data));
        }
    }

    void ProfilerManager::CaptureListenerState(AmListenerID listenerId)
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Listener))
            return;

        if (_dataCollector)
        {
            ProfilerListenerData data = _dataCollector->CollectListenerData(listenerId);
            QueueMessage(std::move(data));
        }
    }

    void ProfilerManager::CapturePerformanceMetrics()
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Performance))
            return;

        if (_dataCollector)
        {
            ProfilerPerformanceData data = _dataCollector->CollectPerformanceData();
            QueueMessage(std::move(data));
        }
    }

    void ProfilerManager::CaptureEvent(const ProfilerEvent& event)
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Events))
            return;

        QueueMessage(ProfilerEvent(event));
    }

    void ProfilerManager::CaptureAllEntities()
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Entity))
            return;

        if (_dataCollector)
        {
            auto entityIds = _dataCollector->GetAllEntityIds();
            for (AmEntityID entityId : entityIds)
            {
                CaptureEntityState(entityId);
            }
        }
    }

    void ProfilerManager::CaptureAllChannels()
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Channel))
            return;

        if (_dataCollector)
        {
            auto channelIds = _dataCollector->GetAllChannelIds();
            for (AmChannelID channelId : channelIds)
            {
                CaptureChannelState(channelId);
            }
        }
    }

    void ProfilerManager::CaptureAllListeners()
    {
        if (!_enabled.load() || !ShouldCaptureCategory(eProfilerCategory_Listener))
            return;

        if (_dataCollector)
        {
            auto listenerIds = _dataCollector->GetAllListenerIds();
            for (AmListenerID listenerId : listenerIds)
            {
                CaptureListenerState(listenerId);
            }
        }
    }

    void ProfilerManager::CaptureFullState()
    {
        if (!_enabled.load())
            return;

        CaptureEngineState();
        CaptureAllEntities();
        CaptureAllChannels();
        CaptureAllListeners();
        CapturePerformanceMetrics();
    }

    bool ProfilerManager::StartNetworkServer()
    {
        if (_networkServer)
        {
            amLogWarning("[ProfilerManager] Network server already running");
            return true;
        }

        _networkServer = std::make_unique<ProfilerServer>();

        if (!_networkServer->Start(_config.mServerPort, _config.mBindAddress, _config.mMaxClients))
        {
            amLogError("[ProfilerManager] Failed to start network server");
            _networkServer.reset();
            return false;
        }

        // Set up server callbacks
        _networkServer->SetOnClientConnected(
            [](ProfilerClientID clientId, const ProfilerClientInfo& info)
            {
                amLogInfo("[ProfilerManager] Client %d connected from %s:%d", clientId, info.mAddress.c_str(), info.mPort);
            });

        _networkServer->SetOnClientDisconnected(
            [](ProfilerClientID clientId, const ProfilerClientInfo& info)
            {
                amLogInfo("[ProfilerManager] Client %d disconnected", clientId);
            });

        _networkServer->SetOnError(
            [](const AmString& error)
            {
                amLogError("[ProfilerManager] Network server error: %s", error.c_str());
            });

        amLogInfo("[ProfilerManager] Network server started on %s:%d", _config.mBindAddress.c_str(), _config.mServerPort);
        return true;
    }

    void ProfilerManager::StopNetworkServer()
    {
        if (_networkServer)
        {
            _networkServer->Stop();
            _networkServer.reset();
            amLogInfo("[ProfilerManager] Network server stopped");
        }
    }

    bool ProfilerManager::IsNetworkServerRunning() const
    {
        return _networkServer != nullptr;
    }

    AmUInt32 ProfilerManager::GetConnectedClientCount() const
    {
        return _networkServer ? _networkServer->GetClientCount() : 0;
    }

    ProfilerManager::Statistics ProfilerManager::GetStatistics() const
    {
        Thread::LockMutex(_statisticsMutex);
        Statistics stats = _statistics;
        Thread::UnlockMutex(_statisticsMutex);
        return stats;
    }

    void ProfilerManager::ResetStatistics()
    {
        Thread::LockMutex(_statisticsMutex);
        _statistics = {};
        Thread::UnlockMutex(_statisticsMutex);
        amLogInfo("[ProfilerManager] Statistics reset");
    }

    void ProfilerManager::RegisterMessageCallback(const MessageCallback& callback)
    {
        Thread::LockMutex(_callbackMutex);
        _localCallback = callback;
        Thread::UnlockMutex(_callbackMutex);
    }

    void ProfilerManager::UnregisterMessageCallback()
    {
        Thread::LockMutex(_callbackMutex);
        _localCallback = nullptr;
        Thread::UnlockMutex(_callbackMutex);
    }

    void ProfilerManager::UpdateLoop()
    {
        amLogDebug("[ProfilerManager] Update loop started");

        while (_running.load())
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration<AmReal32>(currentTime - _lastUpdate).count();

            Thread::LockMutex(_configMutex);
            eProfilerUpdateMode updateMode = _config.mUpdateMode;
            AmReal32 interval = _updateInterval;
            Thread::UnlockMutex(_configMutex);

            bool shouldUpdate = false;
            switch (updateMode)
            {
            case eProfilerUpdateMode_Timed:
                shouldUpdate = deltaTime >= interval;
                break;
            case eProfilerUpdateMode_PerFrame:
                shouldUpdate = true;
                break;
            case eProfilerUpdateMode_OnChange:
                CollectOnChangeUpdates();
                break;
            case eProfilerUpdateMode_Manual:
                // No automatic updates in manual mode
                break;
            }

            if (shouldUpdate)
            {
                CollectTimedUpdates();
                _lastUpdate = currentTime;
            }

            ProcessQueuedMessages();

            // Sleep for a short time to prevent busy waiting
            Thread::Sleep(1);
        }

        amLogDebug("[ProfilerManager] Update loop stopped");
    }

    void ProfilerManager::ProcessQueuedMessages()
    {
        Thread::LockMutex(_configMutex);
        AmUInt32 maxMessages = _config.mMaxMessagesPerFrame;
        Thread::UnlockMutex(_configMutex);

        auto messages = _messageQueue->PopMessages(maxMessages);
        for (const auto& message : messages)
        {
            DistributeMessage(message);
        }
    }

    void ProfilerManager::CollectTimedUpdates()
    {
        if (!_enabled.load())
            return;

        Thread::LockMutex(_configMutex);
        bool captureEngine = _config.mCaptureEngineState;
        bool captureEntities = _config.mCaptureEntityStates;
        bool captureChannels = _config.mCaptureChannelStates;
        bool captureListeners = _config.mCaptureListenerStates;
        bool capturePerformance = _config.mCapturePerformanceMetrics;
        Thread::UnlockMutex(_configMutex);

        if (captureEngine)
            CaptureEngineState();
        if (captureEntities)
            CaptureAllEntities();
        if (captureChannels)
            CaptureAllChannels();
        if (captureListeners)
            CaptureAllListeners();
        if (capturePerformance)
            CapturePerformanceMetrics();
    }

    void ProfilerManager::CollectOnChangeUpdates()
    {
        // TODO: Implement change detection logic
        // This would compare current states with _lastEntityStates, _lastChannelStates, etc.
        // and only send updates when significant changes are detected
    }

    bool ProfilerManager::ShouldCaptureCategory(eProfilerCategory category) const
    {
        Thread::LockMutex(_configMutex);
        bool shouldCapture = (_config.mCategoryMask & static_cast<AmUInt32>(category)) != 0;
        Thread::UnlockMutex(_configMutex);
        return shouldCapture;
    }

    bool ProfilerManager::HasSignificantChange(const ProfilerDataVariant& newData, const ProfilerDataVariant& oldData) const
    {
        // TODO: Implement change detection based on thresholds in config
        return true; // For now, always consider changes significant
    }

    void ProfilerManager::QueueMessage(ProfilerDataVariant&& message)
    {
        if (!_messageQueue->PushMessage(std::move(message)))
        {
            // Queue is full, increment dropped message counter
            Thread::LockMutex(_statisticsMutex);
            _statistics.messagesDropped++;
            Thread::UnlockMutex(_statisticsMutex);

            amLogWarning("[ProfilerManager] Message queue full, dropping message");
        }
    }

    void ProfilerManager::DistributeMessage(const ProfilerDataVariant& message)
    {
        // Update statistics
        Thread::LockMutex(_statisticsMutex);
        _statistics.totalMessagesSent++;
        Thread::UnlockMutex(_statisticsMutex);

        // Send to local callback
        Thread::LockMutex(_callbackMutex);
        if (_localCallback)
        {
            _localCallback(message);
        }
        Thread::UnlockMutex(_callbackMutex);

        // Send to network clients
        if (_networkServer)
            _networkServer->BroadcastProfilerData(message);
    }

    void ProfilerManager::StartUpdateThread()
    {
        if (_updateThread)
        {
            amLogWarning("[ProfilerManager] Update thread already running");
            return;
        }

        _running = true;
        _updateThread = Thread::CreateThread(
            [](AmVoidPtr userData)
            {
                ProfilerManager* manager = static_cast<ProfilerManager*>(userData);
                manager->UpdateLoop();
            },
            this);

        amLogDebug("[ProfilerManager] Update thread started");
    }

    void ProfilerManager::StopUpdateThread()
    {
        if (!_updateThread)
            return;

        _running = false;
        Thread::Wait(_updateThread);
        Thread::Release(_updateThread);

        amLogDebug("[ProfilerManager] Update thread stopped");
    }

} // namespace SparkyStudios::Audio::Amplitude
