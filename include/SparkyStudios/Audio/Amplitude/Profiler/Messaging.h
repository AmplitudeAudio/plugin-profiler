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

#ifndef _AM_PROFILER_MESSAGING_H
#define _AM_PROFILER_MESSAGING_H

#include <SparkyStudios/Audio/Amplitude/Core/Memory.h>
#include <SparkyStudios/Audio/Amplitude/Core/Thread.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Data.h>

namespace SparkyStudios::Audio::Amplitude
{
    /**
     * @brief Thread-safe memory pool for profiler messages.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerMessagePool
    {
    public:
        ProfilerMessagePool(AmSize initialSize = 100, AmSize maxSize = 1000);
        ~ProfilerMessagePool();

        /**
         * @brief Allocate a message from the pool
         */
        template<typename T>
        std::unique_ptr<T> AllocateMessage()
        {
            return AmUniquePtr<T, eMemoryPoolKind_IO>();
        }

        /**
         * @brief Get pool statistics
         */
        struct Stats
        {
            AmSize mAllocatedCount;
            AmSize mAvailableCount;
            AmSize mPeakUsage;
            AmSize mTotalAllocations;
        };

        Stats GetStats() const;
        void Reset();

    private:
        mutable AmMutexHandle _mutex;
        AmSize _initialSize;
        AmSize _maxSize;
        Stats _stats;
    };

    /**
     * @brief Thread-safe message queue for profiler data.
     *
     * @ingroup profiling
     */
    class AM_API_PUBLIC ProfilerMessageQueue
    {
    public:
        explicit ProfilerMessageQueue(AmSize maxSize = 1000);
        ~ProfilerMessageQueue() = default;

        /**
         * @brief Push a message to the queue (non-blocking)
         * @return true if message was queued, false if queue is full
         */
        bool PushMessage(ProfilerDataVariant&& message);

        /**
         * @brief Pop a message from the queue (non-blocking)
         * @return Optional message, empty if queue is empty
         */
        std::optional<ProfilerDataVariant> PopMessage();

        /**
         * @brief Pop multiple messages at once
         * @param maxCount Maximum number of messages to pop
         * @return Vector of messages (may be empty)
         */
        std::vector<ProfilerDataVariant> PopMessages(AmSize maxCount);

        /**
         * @brief Get current queue size
         */
        AmSize Size() const;

        /**
         * @brief Check if queue is empty
         */
        bool Empty() const;

        /**
         * @brief Clear all messages
         */
        void Clear();

    private:
        mutable AmMutexHandle _mutex;
        std::queue<ProfilerDataVariant> _queue;
        AmSize _maxSize;
        std::atomic<AmSize> _currentSize;
        std::atomic<AmSize> _droppedMessages;
    };
} // namespace SparkyStudios::Audio::Amplitude

#endif // _AM_PROFILER_MESSAGING_H
