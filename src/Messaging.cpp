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

#include <SparkyStudios/Audio/Amplitude/IO/Log.h>
#include <SparkyStudios/Audio/Amplitude/Profiler/Messaging.h>

namespace SparkyStudios::Audio::Amplitude
{
    // ProfilerMessagePool implementation
    ProfilerMessagePool::ProfilerMessagePool(AmSize initialSize, AmSize maxSize)
        : _initialSize(initialSize)
        , _maxSize(maxSize)
    {
        _mutex = Thread::CreateMutex();

        Thread::LockMutex(_mutex);
        _stats.mAllocatedCount = 0;
        _stats.mAvailableCount = 0;
        _stats.mPeakUsage = 0;
        _stats.mTotalAllocations = 0;
        Thread::UnlockMutex(_mutex);

        amLogDebug("[ProfilerMessagePool] Created message pool with initial size: %zu, max size: %zu", initialSize, maxSize);
    }

    ProfilerMessagePool::~ProfilerMessagePool()
    {
        if (_mutex)
        {
            Thread::DestroyMutex(_mutex);
            _mutex = nullptr;
        }

        amLogDebug("[ProfilerMessagePool] Destroyed message pool");
    }

    ProfilerMessagePool::Stats ProfilerMessagePool::GetStats() const
    {
        Thread::LockMutex(_mutex);
        Stats stats = _stats;
        Thread::UnlockMutex(_mutex);
        return stats;
    }

    void ProfilerMessagePool::Reset()
    {
        Thread::LockMutex(_mutex);
        _stats.mAllocatedCount = 0;
        _stats.mAvailableCount = 0;
        _stats.mPeakUsage = 0;
        _stats.mTotalAllocations = 0;
        Thread::UnlockMutex(_mutex);

        amLogDebug("[ProfilerMessagePool] Reset message pool statistics");
    }

    // ProfilerMessageQueue implementation
    ProfilerMessageQueue::ProfilerMessageQueue(AmSize maxSize)
        : _maxSize(maxSize)
        , _currentSize(0)
        , _droppedMessages(0)
    {
        _mutex = Thread::CreateMutex();
        amLogDebug("[ProfilerMessageQueue] Created message queue with max size: %zu", maxSize);
    }

    bool ProfilerMessageQueue::PushMessage(ProfilerDataVariant&& message)
    {
        Thread::LockMutex(_mutex);

        if (_queue.size() >= _maxSize)
        {
            _droppedMessages++;
            Thread::UnlockMutex(_mutex);
            amLogWarning("[ProfilerMessageQueue] Queue full, dropping message (total dropped: %zu)", _droppedMessages.load());
            return false;
        }

        _queue.push(std::move(message));
        _currentSize = _queue.size();

        Thread::UnlockMutex(_mutex);
        return true;
    }

    std::optional<ProfilerDataVariant> ProfilerMessageQueue::PopMessage()
    {
        Thread::LockMutex(_mutex);

        if (_queue.empty())
        {
            Thread::UnlockMutex(_mutex);
            return std::nullopt;
        }

        ProfilerDataVariant message = std::move(_queue.front());
        _queue.pop();
        _currentSize = _queue.size();

        Thread::UnlockMutex(_mutex);
        return message;
    }

    std::vector<ProfilerDataVariant> ProfilerMessageQueue::PopMessages(AmSize maxCount)
    {
        std::vector<ProfilerDataVariant> messages;
        messages.reserve(maxCount);

        Thread::LockMutex(_mutex);

        AmSize count = 0;
        while (!_queue.empty() && count < maxCount)
        {
            messages.push_back(std::move(_queue.front()));
            _queue.pop();
            count++;
        }

        _currentSize = _queue.size();

        Thread::UnlockMutex(_mutex);

        if (!messages.empty())
        {
            amLogDebug("[ProfilerMessageQueue] Popped %zu messages from queue", messages.size());
        }

        return messages;
    }

    AmSize ProfilerMessageQueue::Size() const
    {
        return _currentSize.load();
    }

    bool ProfilerMessageQueue::Empty() const
    {
        return _currentSize.load() == 0;
    }

    void ProfilerMessageQueue::Clear()
    {
        Thread::LockMutex(_mutex);

        AmSize clearedCount = _queue.size();
        while (!_queue.empty())
        {
            _queue.pop();
        }
        _currentSize = 0;

        Thread::UnlockMutex(_mutex);

        if (clearedCount > 0)
        {
            amLogDebug("[ProfilerMessageQueue] Cleared %zu messages from queue", clearedCount);
        }
    }

} // namespace SparkyStudios::Audio::Amplitude
