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

#include <SparkyStudios/Audio/Amplitude/Core/Memory.h>
#include <SparkyStudios/Audio/Amplitude/IO/Log.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Server.h>

#include <json/reader.h>
#include <json/writer.h>
#include <uwebsockets/App.h>

namespace SparkyStudios::Audio::Amplitude
{
    struct WebSocketUserData
    {
        ProfilerClientID clientId;
    };

    static AmUniquePtr<uWS::App, eMemoryPoolKind_IO> gSocket = nullptr;
    static uWS::Loop* gLoop = nullptr;

    ProfilerServer::ProfilerServer()
        : _running(false)
        , _initialized(false)
        , _serverSocket(AM_INVALID_SOCKET)
        , _port(0)
        , _bindAddress("127.0.0.1")
        , _maxClients(8)
        , _acceptThread(nullptr)
        , _nextClientId(1)
    {
        _clientsMutex = Thread::CreateMutex();
        _statisticsMutex = Thread::CreateMutex();
        _callbacksMutex = Thread::CreateMutex();

        // Initialize statistics
        Thread::LockMutex(_statisticsMutex);
        _statistics = {};
        _statistics.mServerStartTime = std::chrono::high_resolution_clock::now();
        Thread::UnlockMutex(_statisticsMutex);

        amLogInfo("[ProfilerServer] Created profiler server");
    }

    ProfilerServer::~ProfilerServer()
    {
        Stop();

        if (_clientsMutex)
            Thread::DestroyMutex(_clientsMutex);
        if (_statisticsMutex)
            Thread::DestroyMutex(_statisticsMutex);
        if (_callbacksMutex)
            Thread::DestroyMutex(_callbacksMutex);

        amLogInfo("[ProfilerServer] Destroyed profiler server");
    }

    bool ProfilerServer::Start(AmUInt16 port, const AmString& bindAddress, AmUInt32 maxClients)
    {
        if (_running.load())
        {
            amLogWarning("[ProfilerServer] Server is already running");
            return true;
        }

        _port = port;
        _bindAddress = bindAddress;
        _maxClients = maxClients;

        amLogInfo("[ProfilerServer] Starting server on %s:%d (max clients: %d)", bindAddress.c_str(), port, maxClients);

        // Initialize networking
        if (!_initializeNetworking())
        {
            amLogError("[ProfilerServer] Failed to initialize networking");
            return false;
        }

        // Create server socket
        if (!_createServerSocket())
        {
            amLogError("[ProfilerServer] Failed to create server socket");
            _cleanupNetworking();
            return false;
        }

        // Bind and listen
        if (!_bindAndListen())
        {
            amLogError("[ProfilerServer] Failed to bind and listen");
            _closeServerSocket();
            _cleanupNetworking();
            return false;
        }

        // Start accept thread
        gLoop = uWS::Loop::get();

        _acceptThread = Thread::CreateThread(
            [](AmVoidPtr userData)
            {
                ProfilerServer* server = static_cast<ProfilerServer*>(userData);
                server->_acceptThreadFunction();
            },
            this);

        if (!_acceptThread)
        {
            amLogError("[ProfilerServer] Failed to create accept thread");
            _running = false;
            _closeServerSocket();
            _cleanupNetworking();
            return false;
        }

        _initialized = true;
        amLogInfo("[ProfilerServer] Server started successfully on %s:%d", bindAddress.c_str(), port);

        return true;
    }

    void ProfilerServer::Stop()
    {
        if (!_running.load())
            return;

        amLogInfo("[ProfilerServer] Stopping profiler server");

        _running = false;

        // Disconnect all clients
        _disconnectAllClients();

        // Close server socket to stop accepting new connections
        _closeServerSocket();

        // Cleanup networking
        _cleanupNetworking();

        // Wait for accept thread to finish
        if (_acceptThread)
        {
            Thread::Wait(_acceptThread);
            Thread::Release(_acceptThread);
            _acceptThread = nullptr;
        }

        _initialized = false;
        amLogInfo("[ProfilerServer] Server stopped");
    }

    bool ProfilerServer::IsRunning() const
    {
        return _running.load();
    }

    AmUInt16 ProfilerServer::GetPort() const
    {
        return _port;
    }

    const AmString& ProfilerServer::GetBindAddress() const
    {
        return _bindAddress;
    }

    AmUInt32 ProfilerServer::GetClientCount() const
    {
        Thread::LockMutex(_clientsMutex);
        AmUInt32 count = static_cast<AmUInt32>(_clients.size());
        Thread::UnlockMutex(_clientsMutex);
        return count;
    }

    AmUInt32 ProfilerServer::GetMaxClients() const
    {
        return _maxClients;
    }

    AmUInt32 ProfilerServer::BroadcastMessage(const AmString& jsonMessage)
    {
        AmUInt32 sentCount = 0;

        Thread::LockMutex(_clientsMutex);

        for (auto& pair : _clients)
        {
            ProfilerClientInfo& client = pair.second;
            if (client.mIsConnected && _sendToSocket(client.mSocket, jsonMessage))
            {
                sentCount++;
                client.mMessagesSent++;
                client.mBytesTransmitted += jsonMessage.length();
                _updateStatistics(client.mClientId, jsonMessage.length());
            }
        }

        Thread::UnlockMutex(_clientsMutex);

        if (sentCount > 0)
            amLogInfo("[ProfilerServer] Broadcast message to %d clients (%zu bytes)", sentCount, jsonMessage.length());

        return sentCount;
    }

    bool ProfilerServer::SendMessageToClient(ProfilerClientID clientId, const AmString& jsonMessage)
    {
        Thread::LockMutex(_clientsMutex);

        auto it = _clients.find(clientId);
        if (it == _clients.end() || !it->second.mIsConnected)
        {
            Thread::UnlockMutex(_clientsMutex);
            return false;
        }

        ProfilerClientInfo& client = it->second;
        bool success = _sendToSocket(client.mSocket, jsonMessage);

        if (success)
        {
            client.mMessagesSent++;
            client.mBytesTransmitted += jsonMessage.length();
            _updateStatistics(clientId, jsonMessage.length());
        }

        Thread::UnlockMutex(_clientsMutex);

        return success;
    }

    AmUInt32 ProfilerServer::BroadcastProfilerData(const ProfilerDataVariant& data)
    {
        AmString jsonMessage = _serializeProfilerData(data);
        return BroadcastMessage(jsonMessage);
    }

    bool ProfilerServer::DisconnectClient(ProfilerClientID clientId)
    {
        Thread::LockMutex(_clientsMutex);

        auto clientIt = _clients.find(clientId);
        auto threadIt = _clientThreads.find(clientId);

        if (clientIt == _clients.end())
        {
            Thread::UnlockMutex(_clientsMutex);
            return false;
        }

        ProfilerClientInfo clientInfo = clientIt->second;
        clientInfo.mIsConnected = false;

        // Close socket
        if (clientInfo.mSocket != AM_INVALID_SOCKET)
            _removeClient(clientInfo.mSocket);

        // Clean up thread
        if (threadIt != _clientThreads.end() && threadIt->second)
        {
            Thread::Wait(threadIt->second);
            Thread::Release(threadIt->second);
            _clientThreads.erase(threadIt);
        }

        // Remove from client list
        _clients.erase(clientIt);

        Thread::UnlockMutex(_clientsMutex);

        // Update statistics
        Thread::LockMutex(_statisticsMutex);
        _statistics.mTotalDisconnections++;
        _statistics.mActiveConnections = static_cast<AmUInt32>(_clients.size());
        Thread::UnlockMutex(_statisticsMutex);

        // Trigger disconnection callback
        _triggerEvent(
            [this, clientId, clientInfo]()
            {
                Thread::LockMutex(_callbacksMutex);
                if (_onClientDisconnected)
                {
                    _onClientDisconnected(clientId, clientInfo);
                }
                Thread::UnlockMutex(_callbacksMutex);
            });

        amLogInfo("[ProfilerServer] Client %d removed", clientId);

        return true;
    }

    const ProfilerClientInfo* ProfilerServer::GetClientInfo(ProfilerClientID clientId) const
    {
        Thread::LockMutex(_clientsMutex);

        auto it = _clients.find(clientId);
        const ProfilerClientInfo* info = (it != _clients.end()) ? &it->second : nullptr;

        Thread::UnlockMutex(_clientsMutex);
        return info;
    }

    std::vector<ProfilerClientInfo> ProfilerServer::GetAllClients() const
    {
        std::vector<ProfilerClientInfo> clients;

        Thread::LockMutex(_clientsMutex);
        clients.reserve(_clients.size());

        for (const auto& pair : _clients)
        {
            clients.push_back(pair.second);
        }

        Thread::UnlockMutex(_clientsMutex);
        return clients;
    }

    ProfilerServer::Statistics ProfilerServer::GetStatistics() const
    {
        Thread::LockMutex(_statisticsMutex);
        Statistics stats = _statistics;
        Thread::UnlockMutex(_statisticsMutex);
        return stats;
    }

    void ProfilerServer::ResetStatistics()
    {
        Thread::LockMutex(_statisticsMutex);
        _statistics = {};
        _statistics.mServerStartTime = std::chrono::high_resolution_clock::now();
        Thread::UnlockMutex(_statisticsMutex);

        amLogInfo("[ProfilerServer] Statistics reset");
    }

    void ProfilerServer::SetOnClientConnected(const ClientEventCallback& callback)
    {
        Thread::LockMutex(_callbacksMutex);
        _onClientConnected = callback;
        Thread::UnlockMutex(_callbacksMutex);
    }

    void ProfilerServer::SetOnClientDisconnected(const ClientEventCallback& callback)
    {
        Thread::LockMutex(_callbacksMutex);
        _onClientDisconnected = callback;
        Thread::UnlockMutex(_callbacksMutex);
    }

    void ProfilerServer::SetOnMessageReceived(const MessageEventCallback& callback)
    {
        Thread::LockMutex(_callbacksMutex);
        _onMessageReceived = callback;
        Thread::UnlockMutex(_callbacksMutex);
    }

    void ProfilerServer::SetOnError(const ErrorEventCallback& callback)
    {
        Thread::LockMutex(_callbacksMutex);
        _onError = callback;
        Thread::UnlockMutex(_callbacksMutex);
    }

    bool ProfilerServer::_initializeNetworking()
    {
        gSocket.reset(ampoolnew(eMemoryPoolKind_IO, uWS::App));
        return gSocket.get() != nullptr;
    }

    void ProfilerServer::_cleanupNetworking()
    {
        gSocket.reset(nullptr);
        gLoop = nullptr;
    }

    bool ProfilerServer::_createServerSocket()
    {
        ProfilerServer* self = this;

        gSocket->ws<WebSocketUserData>(
            "/stream",
            { .compression = uWS::SHARED_COMPRESSOR,
              .maxPayloadLength = static_cast<unsigned int>(kMaxMessageSize),
              .idleTimeout = 120,
              .maxBackpressure = 1 * 1024 * 1024,

              .open =
                  [self](auto* ws)
              {
                  AmString address;
                  AmUInt16 port = 0;

                  // Get remote address
                  auto remoteAddr = ws->getRemoteAddressAsText();
                  address = AmString(remoteAddr.data(), remoteAddr.length());

                  // Generate client ID
                  ProfilerClientID clientId = self->_generateClientId();

                  // Store client ID in user data
                  WebSocketUserData* userData = ws->getUserData();
                  userData->clientId = clientId;

                  // Add client
                  self->_addClient(ws, address, port);

                  amLogInfo("[ProfilerServer] Client %d connected from %s", clientId, address.c_str());
              },

              .message =
                  [self](auto* ws, std::string_view message, uWS::OpCode opCode)
              {
                  WebSocketUserData* userData = ws->getUserData();
                  ProfilerClientID clientId = userData->clientId;

                  AmString messageStr(message.data(), message.length());

                  // Trigger message received callback
                  self->_triggerEvent(
                      [self, clientId, messageStr]()
                      {
                          Thread::LockMutex(self->_callbacksMutex);
                          if (self->_onMessageReceived)
                          {
                              self->_onMessageReceived(clientId, messageStr);
                          }
                          Thread::UnlockMutex(self->_callbacksMutex);
                      });

                  amLogInfo("[ProfilerServer] Received message from client %d (%zu bytes)", clientId, message.length());
              },

              .drain =
                  [](auto* ws)
              {
                  // Handle backpressure if needed
              },

              .close =
                  [self](auto* ws, int code, std::string_view message)
              {
                  WebSocketUserData* userData = ws->getUserData();
                  ProfilerClientID clientId = userData->clientId;

                  Thread::LockMutex(self->_clientsMutex);

                  auto it = self->_clients.find(clientId);
                  if (it != self->_clients.end())
                  {
                      ProfilerClientInfo clientInfo = it->second;
                      clientInfo.mIsConnected = false;

                      self->_clients.erase(it);

                      Thread::UnlockMutex(self->_clientsMutex);

                      // Update statistics
                      Thread::LockMutex(self->_statisticsMutex);
                      self->_statistics.mTotalDisconnections++;
                      self->_statistics.mActiveConnections = static_cast<AmUInt32>(self->_clients.size());
                      Thread::UnlockMutex(self->_statisticsMutex);

                      // Trigger disconnection callback
                      self->_triggerEvent(
                          [self, clientId, clientInfo]()
                          {
                              Thread::LockMutex(self->_callbacksMutex);
                              if (self->_onClientDisconnected)
                              {
                                  self->_onClientDisconnected(clientId, clientInfo);
                              }
                              Thread::UnlockMutex(self->_callbacksMutex);
                          });

                      amLogInfo("[ProfilerServer] Client %d disconnected (code: %d)", clientId, code);
                  }
                  else
                  {
                      Thread::UnlockMutex(self->_clientsMutex);
                  }
              } });

        return true;
    }

    void ProfilerServer::_closeServerSocket()
    {
        gSocket->close();
    }

    bool ProfilerServer::_bindAndListen()
    {
        // Actual listening will happen in the accept thread
        // This method is just for compatibility with the existing API
        return true;
    }

    void ProfilerServer::_acceptThreadFunction()
    {
        amLogInfo("[ProfilerServer] Accept thread started");

        // Set up the listen callback and start the server
        ProfilerServer* self = this;

        if (gSocket)
        {
            // Configure listening with callback
            gSocket->listen(
                _port,
                [self](auto* listenSocket)
                {
                    if (listenSocket)
                    {
                        amLogInfo("[ProfilerServer] Successfully listening on port %d", self->_port);
                    }
                    else
                    {
                        amLogError("[ProfilerServer] Failed to listen on port %d", self->_port);
                        self->_triggerEvent(
                            [self]()
                            {
                                Thread::LockMutex(self->_callbacksMutex);
                                if (self->_onError)
                                {
                                    self->_onError("Failed to bind to port");
                                }
                                Thread::UnlockMutex(self->_callbacksMutex);
                            });
                    }
                });

            // Get the current loop before running
            amLogInfo("[ProfilerServer] Starting uWS event loop on port %d...", _port);

            try
            {
                // This should block until the server is stopped
                _running = true;
                gLoop->run();
                amLogInfo("[ProfilerServer] uWS event loop exited");
            } catch (const std::exception& e)
            {
                amLogError("[ProfilerServer] Event loop exception: %s", e.what());
            }

            _running = false;
        }
        else
        {
            amLogError("[ProfilerServer] gSocket is null, cannot run event loop");
        }

        amLogInfo("[ProfilerServer] Accept thread ended");
    }

    void ProfilerServer::_clientThreadFunction(SocketHandle clientSocket, ProfilerClientID clientId)
    {
        // Not used with uWebSockets - handled by event loop
    }

    ProfilerClientID ProfilerServer::_generateClientId()
    {
        return _nextClientId.fetch_add(1);
    }

    void ProfilerServer::_addClient(SocketHandle clientSocket, const AmString& address, AmUInt16 port)
    {
        ProfilerClientID clientId = 0;

        // Extract client ID from socket user data
        if (clientSocket != AM_INVALID_SOCKET)
        {
            auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(clientSocket);
            WebSocketUserData* userData = ws->getUserData();
            clientId = userData->clientId;
        }

        Thread::LockMutex(_clientsMutex);

        // Check if max clients reached
        if (_clients.size() >= _maxClients)
        {
            Thread::UnlockMutex(_clientsMutex);
            amLogWarning("[ProfilerServer] Max clients reached, rejecting connection");

            // Close the socket
            if (clientSocket != AM_INVALID_SOCKET)
            {
                auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(clientSocket);
                ws->close();
            }

            return;
        }

        ProfilerClientInfo info;
        info.mClientId = clientId;
        info.mSocket = clientSocket;
        info.mAddress = address;
        info.mPort = port;
        info.mConnectedTime = std::chrono::high_resolution_clock::now();
        info.mMessagesSent = 0;
        info.mBytesTransmitted = 0;
        info.mIsConnected = true;

        _clients[clientId] = info;

        Thread::UnlockMutex(_clientsMutex);

        // Update statistics
        Thread::LockMutex(_statisticsMutex);
        _statistics.mTotalConnections++;
        _statistics.mActiveConnections = static_cast<AmUInt32>(_clients.size());
        Thread::UnlockMutex(_statisticsMutex);

        // Trigger connection callback
        _triggerEvent(
            [this, clientId, info]()
            {
                Thread::LockMutex(_callbacksMutex);
                if (_onClientConnected)
                {
                    _onClientConnected(clientId, info);
                }
                Thread::UnlockMutex(_callbacksMutex);
            });
    }

    void ProfilerServer::_removeClient(SocketHandle clientSocket)
    {
        if (clientSocket == AM_INVALID_SOCKET)
            return;

        auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(clientSocket);
        ws->close();
    }

    void ProfilerServer::_disconnectAllClients()
    {
        Thread::LockMutex(_clientsMutex);

        std::vector<SocketHandle> socketsToClose;
        for (auto& pair : _clients)
        {
            if (pair.second.mIsConnected && pair.second.mSocket != AM_INVALID_SOCKET)
            {
                socketsToClose.push_back(pair.second.mSocket);
            }
        }

        _clients.clear();

        Thread::UnlockMutex(_clientsMutex);

        // Close all sockets
        for (auto socket : socketsToClose)
        {
            if (gLoop)
            {
                gLoop->defer(
                    [socket]()
                    {
                        auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(socket);
                        ws->close();
                    });
            }
        }

        amLogInfo("[ProfilerServer] Disconnected all clients");
    }

    bool ProfilerServer::_sendToSocket(SocketHandle socket, const AmString& message)
    {
        if (socket == AM_INVALID_SOCKET || message.empty())
            return false;

        bool success = false;

        if (gLoop)
        {
            gLoop->defer(
                [socket, message, &success]()
                {
                    auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(socket);
                    auto result = ws->send(message, uWS::OpCode::TEXT);
                    success = (result != uWS::WebSocket<false, true, WebSocketUserData>::DROPPED);
                });
        }

        return success;
    }

    AmString ProfilerServer::_receiveFromSocket(SocketHandle socket)
    {
        // Not used with uWebSockets - messages come through the .message handler
        return "";
    }

    AmString ProfilerServer::_serializeProfilerData(const ProfilerDataVariant& data)
    {
        Json::Value root;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";

        std::visit(
            [&root](const auto& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                root["timestamp"] = static_cast<Json::UInt64>(
                    std::chrono::duration_cast<std::chrono::microseconds>(arg.mTimestamp.time_since_epoch()).count());
                root["messageId"] = static_cast<Json::UInt64>(arg.mMessageId);
                root["category"] = static_cast<int>(arg.mCategory);
                root["priority"] = static_cast<int>(arg.mPriority);

                if constexpr (std::is_same_v<T, ProfilerEngineData>)
                {
                    root["type"] = "engine";
                    root["isInitialized"] = arg.mIsInitialized;
                    root["engineUptime"] = arg.mEngineUptime;
                    root["configFile"] = arg.mConfigFile;
                    root["totalEntityCount"] = arg.mTotalEntityCount;
                    root["activeEntityCount"] = arg.mActiveEntityCount;
                    root["totalChannelCount"] = arg.mTotalChannelCount;
                    root["activeChannelCount"] = arg.mActiveChannelCount;
                    root["totalListenerCount"] = arg.mTotalListenerCount;
                    root["activeListenerCount"] = arg.mActiveListenerCount;
                    root["cpuUsagePercent"] = arg.mCpuUsagePercent;
                    root["memoryUsageBytes"] = static_cast<Json::UInt64>(arg.mMemoryUsageBytes);
                    root["activeVoiceCount"] = arg.mActiveVoiceCount;
                    root["maxVoiceCount"] = arg.mMaxVoiceCount;
                    root["sampleRate"] = arg.mSampleRate;
                    root["masterGain"] = arg.mMasterGain;
                }
                else if constexpr (std::is_same_v<T, ProfilerEntityData>)
                {
                    root["type"] = "entity";
                    root["entityId"] = static_cast<Json::UInt64>(arg.mEntityId);
                    root["position"] = Json::arrayValue;
                    root["position"].append(arg.mPosition[0]);
                    root["position"].append(arg.mPosition[1]);
                    root["position"].append(arg.mPosition[2]);
                    root["velocity"] = Json::arrayValue;
                    root["velocity"].append(arg.mVelocity[0]);
                    root["velocity"].append(arg.mVelocity[1]);
                    root["velocity"].append(arg.mVelocity[2]);
                    root["activeChannelCount"] = arg.mActiveChannelCount;
                    root["distanceToListener"] = arg.mDistanceToListener;
                    root["obstruction"] = arg.mObstruction;
                    root["occlusion"] = arg.mOcclusion;
                }
                else if constexpr (std::is_same_v<T, ProfilerChannelData>)
                {
                    root["type"] = "channel";
                    root["channelId"] = static_cast<Json::UInt64>(arg.mChannelId);
                    root["playbackState"] = static_cast<int>(arg.mPlaybackState);
                    root["sourceEntityId"] = static_cast<Json::UInt64>(arg.mSourceEntityId);
                    root["soundName"] = arg.mSoundName;
                    root["gain"] = arg.mGain;
                    root["distanceToListener"] = arg.mDistanceToListener;
                }
                else if constexpr (std::is_same_v<T, ProfilerListenerData>)
                {
                    root["type"] = "listener";
                    root["listenerId"] = static_cast<Json::UInt64>(arg.mListenerId);
                    root["position"] = Json::arrayValue;
                    root["position"].append(arg.mPosition[0]);
                    root["position"].append(arg.mPosition[1]);
                    root["position"].append(arg.mPosition[2]);
                    root["gain"] = arg.mGain;
                    root["currentEnvironment"] = arg.mCurrentEnvironment;
                }
                else if constexpr (std::is_same_v<T, ProfilerPerformanceData>)
                {
                    root["type"] = "performance";
                    root["totalCpuUsage"] = arg.mTotalCpuUsage;
                    root["mixerCpuUsage"] = arg.mMixerCpuUsage;
                    root["dspCpuUsage"] = arg.mDspCpuUsage;
                    root["totalAllocatedMemory"] = static_cast<Json::UInt64>(arg.mTotalAllocatedMemory);
                    root["engineMemory"] = static_cast<Json::UInt64>(arg.mEngineMemory);
                    root["processedSamples"] = arg.mProcessedSamples;
                    root["latencyMs"] = arg.mLatencyMs;
                }
                else if constexpr (std::is_same_v<T, ProfilerEvent>)
                {
                    root["type"] = "event";
                    root["eventName"] = arg.mEventName;
                    root["description"] = arg.mDescription;

                    Json::Value params;
                    for (const auto& param : arg.mParameters)
                    {
                        params[param.first] = param.second;
                    }
                    root["parameters"] = params;
                }
            },
            data);

        return Json::writeString(builder, root);
    }

    AmString ProfilerServer::_getSocketAddress(SocketHandle socket, AmUInt16& port)
    {
        if (socket == AM_INVALID_SOCKET)
            return "";

        auto* ws = static_cast<uWS::WebSocket<false, true, WebSocketUserData>*>(socket);
        auto remoteAddr = ws->getRemoteAddressAsText();

        port = 0; // uWebSockets doesn't provide port easily
        return AmString(remoteAddr.data(), remoteAddr.length());
    }

    void ProfilerServer::_updateStatistics(ProfilerClientID clientId, AmSize messageSize)
    {
        Thread::LockMutex(_statisticsMutex);

        _statistics.mTotalMessagesSent++;
        _statistics.mTotalBytesTransmitted += messageSize;

        if (_statistics.mTotalMessagesSent > 0)
        {
            _statistics.mAverageMessageSize =
                static_cast<AmReal32>(_statistics.mTotalBytesTransmitted) / static_cast<AmReal32>(_statistics.mTotalMessagesSent);
        }

        Thread::UnlockMutex(_statisticsMutex);
    }

    void ProfilerServer::_triggerEvent(const std::function<void()>& eventFunction)
    {
        if (eventFunction)
        {
            eventFunction();
        }
    }

} // namespace SparkyStudios::Audio::Amplitude
