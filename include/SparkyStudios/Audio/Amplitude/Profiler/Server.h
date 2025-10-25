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

#ifndef _AM_PROFILER_SERVER_H
#define _AM_PROFILER_SERVER_H

#include <SparkyStudios/Audio/Amplitude/Core/Common.h>
#include <SparkyStudios/Audio/Amplitude/Core/Thread.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Types.h>

#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>

#define AM_INVALID_SOCKET nullptr

namespace SparkyStudios::Audio::Amplitude
{
    typedef AmVoidPtr SocketHandle;

    /**
     * @brief Information about a connected client.
     *
     * @ingroup profiling
     */
    struct AM_API_PUBLIC ProfilerClientInfo
    {
        ProfilerClientID mClientId;
        SocketHandle mSocket;
        AmString mAddress;
        AmUInt16 mPort;
        ProfilerTime mConnectedTime;
        AmUInt64 mMessagesSent;
        AmUInt64 mBytesTransmitted;
        bool mIsConnected;

        ProfilerClientInfo()
            : mClientId(0)
            , mSocket(AM_INVALID_SOCKET)
            , mPort(0)
            , mConnectedTime(std::chrono::high_resolution_clock::now())
            , mMessagesSent(0)
            , mBytesTransmitted(0)
            , mIsConnected(false)
        {}
    };

    /**
     * @brief TCP server for profiler network communication.
     *
     * This class implements a multi-threaded TCP server that accepts connections
     * from Amplitude Studio clients and broadcasts profiler data in JSON format.
     * The server supports multiple concurrent clients and handles connections
     * asynchronously.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerServer
    {
    public:
        /**
         * @brief Client event callback function type.
         */
        using ClientEventCallback = std::function<void(ProfilerClientID, const ProfilerClientInfo&)>;

        /**
         * @brief Message event callback function type.
         */
        using MessageEventCallback = std::function<void(ProfilerClientID, const AmString&)>;

        /**
         * @brief Error event callback function type.
         */
        using ErrorEventCallback = std::function<void(const AmString&)>;

        /**
         * @brief Server statistics.
         */
        struct Statistics
        {
            AmUInt32 mTotalConnections;
            AmUInt32 mActiveConnections;
            AmUInt32 mTotalDisconnections;
            AmUInt64 mTotalMessagesSent;
            AmUInt64 mTotalBytesTransmitted;
            AmUInt32 mFailedSends;
            AmReal32 mAverageMessageSize;
            ProfilerTime mServerStartTime;
        };

        /**
         * @brief Default constructor.
         */
        ProfilerServer();

        /**
         * @brief Destructor.
         */
        ~ProfilerServer();

        // Non-copyable, non-movable
        ProfilerServer(const ProfilerServer&) = delete;
        ProfilerServer& operator=(const ProfilerServer&) = delete;
        ProfilerServer(ProfilerServer&&) = delete;
        ProfilerServer& operator=(ProfilerServer&&) = delete;

        /**
         * @brief Start the server on the specified port.
         *
         * @param port The port to bind to.
         * @param bindAddress The address to bind to (default: "127.0.0.1").
         * @param maxClients Maximum number of concurrent clients (default: 8).
         * @return true if server started successfully, false otherwise.
         */
        bool Start(AmUInt16 port, const AmString& bindAddress = "127.0.0.1", AmUInt32 maxClients = 8);

        /**
         * @brief Stop the server and disconnect all clients.
         */
        void Stop();

        /**
         * @brief Check if the server is currently running.
         *
         * @return true if server is running, false otherwise.
         */
        bool IsRunning() const;

        /**
         * @brief Get the port the server is bound to.
         *
         * @return Server port, or 0 if not running.
         */
        AmUInt16 GetPort() const;

        /**
         * @brief Get the address the server is bound to.
         *
         * @return Server bind address.
         */
        const AmString& GetBindAddress() const;

        /**
         * @brief Get the number of currently connected clients.
         *
         * @return Number of active client connections.
         */
        AmUInt32 GetClientCount() const;

        /**
         * @brief Get maximum number of clients allowed.
         *
         * @return Maximum client count.
         */
        AmUInt32 GetMaxClients() const;

        /**
         * @brief Broadcast a JSON message to all connected clients.
         *
         * @param jsonMessage The JSON message to broadcast.
         * @return Number of clients the message was sent to.
         */
        AmUInt32 BroadcastMessage(const AmString& jsonMessage);

        /**
         * @brief Send a JSON message to a specific client.
         *
         * @param clientId The client ID to send to.
         * @param jsonMessage The JSON message to send.
         * @return true if message was sent successfully, false otherwise.
         */
        bool SendMessageToClient(ProfilerClientID clientId, const AmString& jsonMessage);

        /**
         * @brief Broadcast profiler data to all connected clients.
         *
         * @param data The profiler data to serialize and broadcast.
         * @return Number of clients the message was sent to.
         */
        AmUInt32 BroadcastProfilerData(const ProfilerDataVariant& data);

        /**
         * @brief Disconnect a specific client.
         *
         * @param clientId The client ID to disconnect.
         * @return true if client was disconnected, false if not found.
         */
        bool DisconnectClient(ProfilerClientID clientId);

        /**
         * @brief Get information about a specific client.
         *
         * @param clientId The client ID to query.
         * @return Client information, or nullptr if not found.
         */
        const ProfilerClientInfo* GetClientInfo(ProfilerClientID clientId) const;

        /**
         * @brief Get information about all connected clients.
         *
         * @return Vector of client information for all connected clients.
         */
        std::vector<ProfilerClientInfo> GetAllClients() const;

        /**
         * @brief Get server statistics.
         *
         * @return Current server statistics.
         */
        Statistics GetStatistics() const;

        /**
         * @brief Reset server statistics.
         */
        void ResetStatistics();

        // Event callbacks

        /**
         * @brief Set callback for client connection events.
         *
         * @param callback Function to call when a client connects.
         */
        void SetOnClientConnected(const ClientEventCallback& callback);

        /**
         * @brief Set callback for client disconnection events.
         *
         * @param callback Function to call when a client disconnects.
         */
        void SetOnClientDisconnected(const ClientEventCallback& callback);

        /**
         * @brief Set callback for incoming message events.
         *
         * @param callback Function to call when a message is received from a client.
         */
        void SetOnMessageReceived(const MessageEventCallback& callback);

        /**
         * @brief Set callback for error events.
         *
         * @param callback Function to call when an error occurs.
         */
        void SetOnError(const ErrorEventCallback& callback);

    private:
        // Server lifecycle
        bool _initializeNetworking();
        void _cleanupNetworking();
        bool _createServerSocket();
        void _closeServerSocket();
        bool _bindAndListen();

        // Threading
        void _acceptThreadFunction();
        void _clientThreadFunction(SocketHandle clientSocket, ProfilerClientID clientId);

        // Client management
        ProfilerClientID _generateClientId();
        void _addClient(SocketHandle clientSocket, const AmString& address, AmUInt16 port);
        void _removeClient(SocketHandle clientSocket);
        void _disconnectAllClients();

        // Message handling
        bool _sendToSocket(SocketHandle socket, const AmString& message);
        AmString _receiveFromSocket(SocketHandle socket);
        AmString _serializeProfilerData(const ProfilerDataVariant& data);

        // Utility functions
        AmString _getSocketAddress(SocketHandle socket, AmUInt16& port);
        void _updateStatistics(ProfilerClientID clientId, AmSize messageSize);
        void _triggerEvent(const std::function<void()>& eventFunction);

        // Member variables
        std::atomic<bool> _running;
        std::atomic<bool> _initialized;

        SocketHandle _serverSocket;
        AmUInt16 _port;
        AmString _bindAddress;
        AmUInt32 _maxClients;

        // Threading
        AmThreadHandle _acceptThread;
        mutable AmMutexHandle _clientsMutex;
        mutable AmMutexHandle _statisticsMutex;
        mutable AmMutexHandle _callbacksMutex;

        // Client management
        std::unordered_map<ProfilerClientID, ProfilerClientInfo> _clients;
        std::unordered_map<ProfilerClientID, AmThreadHandle> _clientThreads;
        std::atomic<ProfilerClientID> _nextClientId;

        // Statistics
        Statistics _statistics;

        // Event callbacks
        ClientEventCallback _onClientConnected;
        ClientEventCallback _onClientDisconnected;
        MessageEventCallback _onMessageReceived;
        ErrorEventCallback _onError;

        // Platform-specific data
#if AM_PLATFORM_WINDOWS
        bool _wsaInitialized;
#endif

        // Constants
        static constexpr AmSize kMaxMessageSize = 1024 * 1024; // 1MB max message size
        static constexpr AmInt32 kSocketReceiveTimeout = 5000; // 5 seconds
        static constexpr AmInt32 kSocketSendTimeout = 5000; // 5 seconds
        static constexpr AmInt32 kListenBacklog = 10;
    };

} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_SERVER_H
