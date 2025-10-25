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

#include <SparkyStudios/Audio/Amplitude/IO/File.h>
#include <SparkyStudios/Audio/Amplitude/IO/Log.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Config.h>

#include <json/reader.h>
#include <json/writer.h>

#include <fstream>

namespace SparkyStudios::Audio::Amplitude
{
    namespace
    {
        AmString UpdateModeToString(eProfilerUpdateMode mode)
        {
            switch (mode)
            {
            case eProfilerUpdateMode_Timed:
                return "timed";
            case eProfilerUpdateMode_OnChange:
                return "on_change";
            case eProfilerUpdateMode_PerFrame:
                return "per_frame";
            case eProfilerUpdateMode_Manual:
                return "manual";
            default:
                return "timed";
            }
        }

        eProfilerUpdateMode StringToUpdateMode(const AmString& str)
        {
            if (str == "timed")
                return eProfilerUpdateMode_Timed;
            if (str == "on_change")
                return eProfilerUpdateMode_OnChange;
            if (str == "per_frame")
                return eProfilerUpdateMode_PerFrame;
            if (str == "manual")
                return eProfilerUpdateMode_Manual;
            return eProfilerUpdateMode_Timed;
        }

        AmString LogLevelToString(eLogMessageLevel level)
        {
            switch (level)
            {
            case eLogMessageLevel_Debug:
                return "debug";
            case eLogMessageLevel_Info:
                return "info";
            case eLogMessageLevel_Warning:
                return "warning";
            case eLogMessageLevel_Error:
                return "error";
            case eLogMessageLevel_Critical:
                return "critical";
            case eLogMessageLevel_Success:
                return "success";
            default:
                return "debug";
            }
        }

        eLogMessageLevel StringToLogLevel(const AmString& str)
        {
            if (str == "debug")
                return eLogMessageLevel_Debug;
            if (str == "info")
                return eLogMessageLevel_Info;
            if (str == "warning")
                return eLogMessageLevel_Warning;
            if (str == "error")
                return eLogMessageLevel_Error;
            if (str == "critical")
                return eLogMessageLevel_Critical;
            if (str == "success")
                return eLogMessageLevel_Success;
            return eLogMessageLevel_Debug;
        }
    } // namespace

    bool ProfilerConfig::LoadFromFile(const AmOsString& configFile)
    {
        std::ifstream file(configFile);
        if (!file.is_open())
        {
            amLogError("[ProfilerConfig] Failed to open config file: %s", configFile.c_str());
            return false;
        }

        Json::Value json;

        Json::CharReaderBuilder reader;
        reader["collectComments"] = false;

        std::string errors;
        bool success = Json::parseFromStream(reader, file, &json, &errors);

        if (!success)
        {
            amLogError("[ProfilerConfig] Failed to parse JSON from config file: %s.\nErrors: %s", configFile.c_str(), errors.c_str());
            return false;
        }

        // Load network settings
        mEnableNetworking = json.get("enable_networking", mEnableNetworking).asBool();
        mServerPort = static_cast<AmUInt16>(json.get("server_port", mServerPort).asUInt());
        mMaxClients = static_cast<AmUInt32>(json.get("max_clients", mMaxClients).asUInt());
        mBindAddress = json.get("bind_address", mBindAddress).asString();

        // Load update settings
        mUpdateMode = StringToUpdateMode(json.get("update_mode", UpdateModeToString(mUpdateMode)).asString());
        mUpdateFrequencyHz = json.get("update_frequency_hz", mUpdateFrequencyHz).asFloat();
        mMaxMessagesPerFrame = static_cast<AmUInt32>(json.get("max_messages_per_frame", mMaxMessagesPerFrame).asUInt());

        // Load data capture settings
        mCategoryMask = static_cast<AmUInt32>(json.get("category_mask", mCategoryMask).asUInt());
        mCaptureEngineState = json.get("capture_engine_state", mCaptureEngineState).asBool();
        mCaptureEntityStates = json.get("capture_entity_states", mCaptureEntityStates).asBool();
        mCaptureChannelStates = json.get("capture_channel_states", mCaptureChannelStates).asBool();
        mCaptureListenerStates = json.get("capture_listener_states", mCaptureListenerStates).asBool();
        mCapturePerformanceMetrics = json.get("capture_performance_metrics", mCapturePerformanceMetrics).asBool();
        mCaptureEvents = json.get("capture_events", mCaptureEvents).asBool();

        // Load performance settings
        mMessageBufferSize = static_cast<AmUInt32>(json.get("message_buffer_size", mMessageBufferSize).asUInt());
        mMaxQueuedMessages = static_cast<AmUInt32>(json.get("max_queued_messages", mMaxQueuedMessages).asUInt());
        mUseCompressionForNetwork = json.get("use_compression_for_network", mUseCompressionForNetwork).asBool();

        // Load filtering settings
        mPositionChangeThreshold = json.get("position_change_threshold", mPositionChangeThreshold).asFloat();
        mOrientationChangeThreshold = json.get("orientation_change_threshold", mOrientationChangeThreshold).asFloat();
        mParameterChangeThreshold = json.get("parameter_change_threshold", mParameterChangeThreshold).asFloat();

        // Load debug settings
        mEnableLogging = json.get("enable_logging", mEnableLogging).asBool();
        mLoggingLevel = StringToLogLevel(json.get("logging_level", LogLevelToString(mLoggingLevel)).asString());
        mLogFilePath = json.get("log_file_path", mLogFilePath).asString();

        amLogInfo("[ProfilerConfig] Configuration loaded successfully from: %s", configFile.c_str());
        return true;
    }

    bool ProfilerConfig::SaveToFile(const AmOsString& configFile) const
    {
        Json::Value json;

        // Save network settings
        json["enable_networking"] = mEnableNetworking;
        json["server_port"] = mServerPort;
        json["max_clients"] = mMaxClients;
        json["bind_address"] = mBindAddress;

        // Save update settings
        json["update_mode"] = UpdateModeToString(mUpdateMode);
        json["update_frequency_hz"] = mUpdateFrequencyHz;
        json["max_messages_per_frame"] = mMaxMessagesPerFrame;

        // Save data capture settings
        json["category_mask"] = mCategoryMask;
        json["capture_engine_state"] = mCaptureEngineState;
        json["capture_entity_states"] = mCaptureEntityStates;
        json["capture_channel_states"] = mCaptureChannelStates;
        json["capture_listener_states"] = mCaptureListenerStates;
        json["capture_performance_metrics"] = mCapturePerformanceMetrics;
        json["capture_events"] = mCaptureEvents;

        // Save performance settings
        json["message_buffer_size"] = mMessageBufferSize;
        json["max_queued_messages"] = mMaxQueuedMessages;
        json["use_compression_for_network"] = mUseCompressionForNetwork;

        // Save filtering settings
        json["position_change_threshold"] = mPositionChangeThreshold;
        json["orientation_change_threshold"] = mOrientationChangeThreshold;
        json["parameter_change_threshold"] = mParameterChangeThreshold;

        // Save debug settings
        json["enable_logging"] = mEnableLogging;
        json["logging_level"] = LogLevelToString(mLoggingLevel);
        json["log_file_path"] = mLogFilePath;

        // Write to file
        std::ofstream file(configFile);
        if (!file.is_open())
        {
            amLogError("[ProfilerConfig] Failed to create config file: %s", configFile.c_str());
            return false;
        }

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "\t";

        file << Json::writeString(wbuilder, json);
        file.close();

        amLogInfo("[ProfilerConfig] Configuration saved successfully to: %s", configFile.c_str());
        return true;
    }

    bool ProfilerConfig::Validate() const
    {
        // Validate network settings
        if (mEnableNetworking)
        {
            if (mServerPort == 0)
            {
                amLogError("[ProfilerConfig] Invalid server port: %d", mServerPort);
                return false;
            }

            if (mMaxClients == 0 || mMaxClients > kMaxProfilerClients)
            {
                amLogError("[ProfilerConfig] Invalid max clients: %d (must be 1-%d)", mMaxClients, kMaxProfilerClients);
                return false;
            }

            if (mBindAddress.empty())
            {
                amLogError("[ProfilerConfig] Bind address cannot be empty when networking is enabled");
                return false;
            }
        }

        // Validate update settings
        if (mUpdateFrequencyHz <= 0.0f || mUpdateFrequencyHz > 1000.0f)
        {
            amLogError("[ProfilerConfig] Invalid update frequency: %f (must be 0.1-1000.0 Hz)", mUpdateFrequencyHz);
            return false;
        }

        if (mMaxMessagesPerFrame == 0 || mMaxMessagesPerFrame > 10000)
        {
            amLogError("[ProfilerConfig] Invalid max messages per frame: %d (must be 1-10000)", mMaxMessagesPerFrame);
            return false;
        }

        // Validate performance settings
        if (mMessageBufferSize < 1024) // Minimum 1KB
        {
            amLogError("[ProfilerConfig] Message buffer size too small: %d (minimum 1024 bytes)", mMessageBufferSize);
            return false;
        }

        if (mMaxQueuedMessages == 0 || mMaxQueuedMessages > 100000)
        {
            amLogError("[ProfilerConfig] Invalid max queued messages: %d (must be 1-100000)", mMaxQueuedMessages);
            return false;
        }

        // Validate filtering settings
        if (mPositionChangeThreshold < 0.0f || mPositionChangeThreshold > 1000.0f)
        {
            amLogError("[ProfilerConfig] Invalid position change threshold: %f (must be 0-1000)", mPositionChangeThreshold);
            return false;
        }

        if (mOrientationChangeThreshold < 0.0f || mOrientationChangeThreshold > 3.14159f) // PI radians
        {
            amLogError("[ProfilerConfig] Invalid orientation change threshold: %f (must be 0-π)", mOrientationChangeThreshold);
            return false;
        }

        if (mParameterChangeThreshold < 0.0f || mParameterChangeThreshold > 1.0f)
        {
            amLogError("[ProfilerConfig] Invalid parameter change threshold: %f (must be 0-1)", mParameterChangeThreshold);
            return false;
        }

        // Validate debug settings
        if (mEnableLogging && mLogFilePath.empty())
        {
            amLogError("[ProfilerConfig] Log file path cannot be empty when logging is enabled");
            return false;
        }

        return true;
    }
} // namespace SparkyStudios::Audio::Amplitude
