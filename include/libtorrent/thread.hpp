/*

Copyright (c) 2009-2016, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_THREAD_HPP_INCLUDED
#define TORRENT_THREAD_HPP_INCLUDED

#include "libtorrent/config.hpp"
#include "libtorrent/assert.hpp"
#include "libtorrent/time.hpp"

#include "libtorrent/aux_/disable_warnings_push.hpp"

#if defined TORRENT_WINDOWS || defined TORRENT_CYGWIN
// asio assumes that the windows error codes are defined already
#include <winsock2.h>
#endif

#if defined TORRENT_BEOS
#include <kernel/OS.h>
#endif

#include <memory> // for auto_ptr required by asio

#include <boost/thread/thread.hpp>
#include <boost/asio/detail/mutex.hpp>
#include <boost/asio/detail/event.hpp>
#include <boost/cstdint.hpp>
#include <boost/atomic.hpp>

#include "libtorrent/aux_/disable_warnings_pop.hpp"

namespace libtorrent
{
	typedef boost::thread thread;
	typedef boost::asio::detail::mutex mutex;
	typedef boost::asio::detail::event event;

	// internal
	void sleep(int milliseconds);

	struct shared_lock
	{
		enum lock_type_t
		{
			shared = 1,
			exclusive = 2
		};

		struct scoped_lock
		{
			scoped_lock(shared_lock& lock, const int type, const bool locked = true)
				: m_lock(lock), m_type(type), m_locked(locked)
			{
				if (m_locked) m_lock.lock(m_type);
			}

			~scoped_lock()
			{
				unlock();
			}

			void lock()
			{
				TORRENT_ASSERT(m_type == shared_lock::shared ||
					m_type == shared_lock::exclusive);
				m_lock.lock(m_type);
				m_locked = true;
			}

			void unlock()
			{
				TORRENT_ASSERT(m_type == shared_lock::shared ||
					m_type == shared_lock::exclusive);
				if (m_locked)
				{
					m_locked = false;
					m_lock.unlock(m_type);
				}
			}

			shared_lock& m_lock;
			int m_type;
			bool m_locked;
		};

		shared_lock() : m_shared_locks(0), m_exclusive_lock(false) {}

		void lock(int type)
		{
			if (type == shared_lock::exclusive)
			{
				m_mutex.lock();
				m_exclusive_lock.store(true);

				// wait for all shared locks to be released
				while (m_shared_locks.load() > 0)
					boost::this_thread::yield();
			}
			else
			{
				TORRENT_ASSERT(type == shared_lock::shared);
				m_shared_locks.fetch_add(1);

				if (m_exclusive_lock.load())
				{
					// if an exclusive lock has been requested by
					// another thread lock the mutex before acquiring
					// a shared lock
					m_shared_locks.fetch_add(-1);
					mutex::scoped_lock lock(m_mutex);
					m_shared_locks.fetch_add(1);
				}
			}
		}

		void unlock(int type)
		{
			if (type == shared_lock::exclusive)
			{
				m_exclusive_lock.store(false);
				m_mutex.unlock();
			}
			else
			{
				TORRENT_ASSERT(type == shared_lock::shared);
				m_shared_locks.fetch_add(-1);
			}
		}

		boost::atomic<int> m_shared_locks;
		boost::atomic<bool> m_exclusive_lock;
		mutex m_mutex;
	};

	struct TORRENT_EXTRA_EXPORT condition_variable
	{
		condition_variable();
		~condition_variable();
		void wait(mutex::scoped_lock& l);
		void wait_for(mutex::scoped_lock& l, time_duration rel_time);
		void notify_all();
		void notify();
	private:
#ifdef BOOST_HAS_PTHREADS
		pthread_cond_t m_cond;
#elif defined TORRENT_WINDOWS || defined TORRENT_CYGWIN
		HANDLE m_sem;
		mutex m_mutex;
		int m_num_waiters;
#elif defined TORRENT_BEOS
		sem_id m_sem;
		mutex m_mutex;
		int m_num_waiters;
#else
#error not implemented
#endif
	};
}

#endif

