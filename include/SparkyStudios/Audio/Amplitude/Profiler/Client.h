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

#ifndef _AM_PROFILER_CLIENT_H
#define _AM_PROFILER_CLIENT_H

#include <SparkyStudios/Audio/Amplitude/Core/Common.h>
#include <SparkyStudios/Audio/Amplitude/Core/Thread.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Types.h>

#include <atomic>
#include <functional>
#include <string>
#include <vector>

// Platform-specific includes
#if AM_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
#define AM_INVALID_SOCKET INVALID_SOCKET
#define AM_SOCKET_ERROR SOCKET_ERROR
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#define AM_INVALID_SOCKET -1
#define AM_SOCKET_ERROR -1
#endif

namespace SparkyStudios::Audio::Amplitude
{
    /**
     * @brief Connection state of the profiler client.
     *
     * @ingroup profiling
     */
    enum eProfilerClientState : AmUInt8
    {
        /**
         * @brief Client is disconnected.
         */
        eProfilerClientState_Disconnected = 0,

        /**
         * @brief Client is attempting to connect.
         */
        eProfilerClientState_Connecting = 1,

        /**
         * @brief Client is connected and ready.
         */
        eProfilerClientState_Connected = 2,

        /**
         * @brief Client is disconnecting.
         */
        eProfilerClientState_Disconnecting = 3,

        /**
         * @brief Client encountered an error.
         */
        eProfilerClientState_Error = 4
    };

    /**
     * @brief Configuration for the profiler client.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerClientConfig
    {
        AmString mServerAddress; ///< Server address to connect to
        AmUInt16 mServerPort; ///< Server port to connect to
        AmString mClientName; ///< Name of this client (for identification)
        AmString mClientVersion; ///< Version of this client
        AmUInt32 mConnectTimeoutMs; ///< Connection timeout in milliseconds
        AmUInt32 mReceiveTimeoutMs; ///< Receive timeout in milliseconds
        AmUInt32 mHeartbeatIntervalMs; ///< Heartbeat interval in milliseconds
        bool mAutoReconnect; ///< Whether to automatically reconnect on disconnect
        AmUInt32 mMaxReconnectAttempts; ///< Maximum number of reconnection attempts
        AmUInt32 mReconnectDelayMs; ///< Delay between reconnection attempts

        ProfilerClientConfig()
            : mServerAddress("127.0.0.1")
            , mServerPort(kDefaultProfilerPort)
            , mClientName("Amplitude Studio")
            , mClientVersion("1.0.0")
            , mConnectTimeoutMs(5000)
            , mReceiveTimeoutMs(10000)
            , mHeartbeatIntervalMs(30000)
            , mAutoReconnect(true)
            , mMaxReconnectAttempts(5)
            , mReconnectDelayMs(2000)
        {}
    };

    /**
     * @brief TCP client for connecting to Amplitude profiler server.
     *
     * This client connects to a ProfilerServer instance and receives real-time
     * profiler data. It provides type-safe callbacks for different kinds of
     * profiler messages and handles connection management automatically.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerClient
    {
    public:
        /**
         * @brief Callback function type for engine data updates.
         */
        using EngineDataCallback = std::function<void(const ProfilerEngineData&)>;

        /**
         * @brief Callback function type for entity data updates.
         */
        using EntityDataCallback = std::function<void(const ProfilerEntityData&)>;

        /**
         * @brief Callback function type for channel data updates.
         */
        using ChannelDataCallback = std::function<void(const ProfilerChannelData&)>;

        /**
         * @brief Callback function type for listener data updates.
         */
        using ListenerDataCallback = std::function<void(const ProfilerListenerData&)>;

        /**
         * @brief Callback function type for performance data updates.
         */
        using PerformanceDataCallback = std::function<void(const ProfilerPerformanceData&)>;

        /**
         * @brief Callback function type for event updates.
         */
        using EventCallback = std::function<void(const ProfilerEvent&)>;

        /**
         * @brief Callback function type for connection state changes.
         */
        using ConnectionStateCallback = std::function<void(eProfilerClientState)>;

        /**
         * @brief Callback function type for error events.
         */
        using ErrorCallback = std::function<void(const AmString&)>;

        /**
         * @brief Callback function type for raw message events.
         */
        using RawMessageCallback = std::function<void(const AmString&)>;

        /**
         * @brief Client statistics.
         */
        struct Statistics
        {
            AmUInt64 mTotalMessagesReceived;
            AmUInt64 mTotalBytesReceived;
            AmUInt64 mMessageParseErrors;
            AmUInt32 mReconnectionAttempts;
            AmReal32 mAverageMessageSize;
            ProfilerTime mConnectedTime;
            ProfilerTime mLastMessageTime;
        };

        /**
         * @brief Default constructor.
         */
        ProfilerClient();

        /**
         * @brief Constructor with configuration.
         */
        explicit ProfilerClient(const ProfilerClientConfig& config);

        /**
         * @brief Destructor.
         */
        ~ProfilerClient();

        // Non-copyable, non-movable
        ProfilerClient(const ProfilerClient&) = delete;
        ProfilerClient& operator=(const ProfilerClient&) = delete;
        ProfilerClient(ProfilerClient&&) = delete;
        ProfilerClient& operator=(ProfilerClient&&) = delete;

        /**
         * @brief Set client configuration.
         *
         * @param config The configuration to use.
         * @note Configuration changes only take effect after reconnection.
         */
        void SetConfig(const ProfilerClientConfig& config);

        /**
         * @brief Get current client configuration.
         *
         * @return Current configuration.
         */
        const ProfilerClientConfig& GetConfig() const;

        /**
         * @brief Connect to the profiler server.
         *
         * @return true if connection was initiated successfully, false otherwise.
         * @note This is an asynchronous operation. Use connection state callbacks to monitor progress.
         */
        bool Connect();

        /**
         * @brief Disconnect from the profiler server.
         */
        void Disconnect();

        /**
         * @brief Check if the client is currently connected.
         *
         * @return true if connected, false otherwise.
         */
        bool IsConnected() const;

        /**
         * @brief Get the current connection state.
         *
         * @return Current connection state.
         */
        eProfilerClientState GetConnectionState() const;

        /**
         * @brief Send a command to the server.
         *
         * @param command The command to send (JSON format).
         * @return true if the command was sent successfully, false otherwise.
         */
        bool SendCommand(const AmString& command);

        /**
         * @brief Send a raw JSON message to the server.
         *
         * @param jsonMessage The JSON message to send.
         * @return true if the message was sent successfully, false otherwise.
         */
        bool SendMessage(const AmString& jsonMessage);

        /**
         * @brief Request specific data from the server.
         *
         * @param dataType The type of data to request ("engine", "entities", "channels", etc.).
         * @return true if the request was sent successfully, false otherwise.
         */
        bool RequestData(const AmString& dataType);

        /**
         * @brief Get client statistics.
         *
         * @return Current client statistics.
         */
        Statistics GetStatistics() const;

        /**
         * @brief Reset client statistics.
         */
        void ResetStatistics();

        // Callback setters

        /**
         * @brief Set callback for engine data updates.
         *
         * @param callback Function to call when engine data is received.
         */
        void SetOnEngineData(const EngineDataCallback& callback);

        /**
         * @brief Set callback for entity data updates.
         *
         * @param callback Function to call when entity data is received.
         */
        void SetOnEntityData(const EntityDataCallback& callback);

        /**
         * @brief Set callback for channel data updates.
         *
         * @param callback Function to call when channel data is received.
         */
        void SetOnChannelData(const ChannelDataCallback& callback);

        /**
         * @brief Set callback for listener data updates.
         *
         * @param callback Function to call when listener data is received.
         */
        void SetOnListenerData(const ListenerDataCallback& callback);

        /**
         * @brief Set callback for performance data updates.
         *
         * @param callback Function to call when performance data is received.
         */
        void SetOnPerformanceData(const PerformanceDataCallback& callback);

        /**
         * @brief Set callback for event updates.
         *
         * @param callback Function to call when events are received.
         */
        void SetOnEvent(const EventCallback& callback);

        /**
         * @brief Set callback for connection state changes.
         *
         * @param callback Function to call when connection state changes.
         */
        void SetOnConnectionStateChanged(const ConnectionStateCallback& callback);

        /**
         * @brief Set callback for error events.
         *
         * @param callback Function to call when errors occur.
         */
        void SetOnError(const ErrorCallback& callback);

        /**
         * @brief Set callback for raw message events.
         *
         * @param callback Function to call for all received messages (before parsing).
         */
        void SetOnRawMessage(const RawMessageCallback& callback);

    private:
        // Connection management
        bool _initializeNetworking();
        void _cleanupNetworking();
        bool _createSocket();
        void _closeSocket();
        bool _connectToServer();
        void _setConnectionState(eProfilerClientState state);

        // Threading
        void _receiveThreadFunction();
        void _heartbeatThreadFunction();
        void _reconnectThreadFunction();

        // Message handling
        AmString _receiveMessage();
        bool _sendMessage(const AmString& message);
        void _processMessage(const AmString& message);
        void _handleHandshake(const AmString& message);
        void _handleProfilerData(const AmString& message);
        void _triggerCallback(const std::function<void()>& callback);

        // Reconnection logic
        void _startReconnection();
        void _stopReconnection();
        bool _shouldReconnect() const;

        // Utility functions
        void _updateStatistics(AmSize messageSize);
        AmString _createHandshakeMessage() const;
        AmString _createHeartbeatMessage() const;

        // Member variables
        std::atomic<eProfilerClientState> _connectionState;
        ProfilerClientConfig _config;

        SocketHandle _socket;
        mutable AmMutexHandle _configMutex;
        mutable AmMutexHandle _statisticsMutex;
        mutable AmMutexHandle _callbacksMutex;

        // Threading
        AmThreadHandle _receiveThread;
        AmThreadHandle _heartbeatThread;
        AmThreadHandle _reconnectThread;
        std::atomic<bool> _running;
        std::atomic<bool> _shouldReconnect;

        // Reconnection state
        std::atomic<AmUInt32> _reconnectAttempts;

        // Statistics
        Statistics _statistics;

        // Callbacks
        EngineDataCallback _onEngineData;
        EntityDataCallback _onEntityData;
        ChannelDataCallback _onChannelData;
        ListenerDataCallback _onListenerData;
        PerformanceDataCallback _onPerformanceData;
        EventCallback _onEvent;
        ConnectionStateCallback _onConnectionStateChanged;
        ErrorCallback _onError;
        RawMessageCallback _onRawMessage;

        // Platform-specific data
#if AM_PLATFORM_WINDOWS
        bool _wsaInitialized;
#endif

        // Constants
        static constexpr AmSize kMaxMessageSize = 1024 * 1024; // 1MB max message size
        static constexpr AmInt32 kSocketReceiveTimeout = 10000; // 10 seconds
        static constexpr AmInt32 kSocketSendTimeout = 5000; // 5 seconds
    };

    /**
     * @brief Convert connection state to string.
     *
     * @param state The connection state to convert.
     * @return String representation of the state.
     */
    AM_API_PUBLIC AmString ProfilerClientStateToString(eProfilerClientState state);

} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_CLIENT_H
