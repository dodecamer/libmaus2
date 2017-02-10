/*
    libmaus2
    Copyright (C) 2009-2015 German Tischler
    Copyright (C) 2011-2013 Genome Research Limited

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if ! defined(TERMINATABLESYNCHRONOUSQUEUE_HPP)
#define TERMINATABLESYNCHRONOUSQUEUE_HPP

#include <libmaus2/parallel/SynchronousQueue.hpp>

#if defined(LIBMAUS2_HAVE_PTHREADS)
namespace libmaus2
{
        namespace parallel
        {
                /**
                 * posix condition variable based version
                 **/
                template<typename _value_type>
                struct TerminatableSynchronousQueue
                {
                        typedef _value_type value_type;
                        typedef TerminatableSynchronousQueue<value_type> this_type;

                        pthread_mutex_t mutex;
                        pthread_cond_t cond;
                        size_t volatile numwait;
                        std::deque<value_type> Q;
                        bool volatile terminated;

                        struct MutexLock
                        {
                                pthread_mutex_t * mutex;
                                bool locked;

                                void obtain()
                                {
                                        if ( ! locked )
                                        {
                                                int const r = pthread_mutex_lock(mutex);
                                                if ( r != 0 )
                                                {
                                                        int const error = errno;
                                                        libmaus2::exception::LibMausException lme;
                                                        lme.getStream() << "MutexLock: " << strerror(error) << std::endl;
                                                        lme.finish();
                                                        throw lme;
                                                }
                                                locked = true;
                                        }
                                }

                                MutexLock(pthread_mutex_t & rmutex) : mutex(&rmutex), locked(false)
                                {
                                        obtain();
                                }

                                void release()
                                {
                                        if ( locked )
                                        {
                                                int const r = pthread_mutex_unlock(mutex);
                                                if ( r != 0 )
                                                {
                                                        int const error = errno;
                                                        libmaus2::exception::LibMausException lme;
                                                        lme.getStream() << "~MutexLock: " << strerror(error) << std::endl;
                                                        lme.finish();
                                                        throw lme;
                                                }

                                                locked = false;
                                        }
                                }

                                ~MutexLock()
                                {
                                        release();
                                }
                        };

			void initCond()
			{
				if ( pthread_cond_init(&cond,NULL) != 0 )
				{
					int const error = errno;
					libmaus2::exception::LibMausException lme;
					lme.getStream() << "PosixConditionSemaphore::initCond(): failed pthread_cond_init " << strerror(error) << std::endl;
					lme.finish();
					throw lme;
				}
			}

			void initMutex()
			{
				if ( pthread_mutex_init(&mutex,NULL) != 0 )
				{
					int const error = errno;
					libmaus2::exception::LibMausException lme;
					lme.getStream() << "PosixConditionSemaphore::initMutex(): failed pthread_mutex_init " << strerror(error) << std::endl;
					lme.finish();
					throw lme;
				}
			}


                        TerminatableSynchronousQueue()
                        : numwait(0), Q(), terminated(false)
                        {
                        	initMutex();
                        	try
                        	{
	                        	initCond();
				}
				catch(...)
				{
					pthread_mutex_destroy(&mutex);
					throw;
				}
                        }

                        ~TerminatableSynchronousQueue()
                        {
				pthread_mutex_destroy(&mutex);
				pthread_cond_destroy(&cond);
                        }

                        void enque(value_type const v)
                        {
                                MutexLock M(mutex);
                                Q.push_back(v);
                                pthread_cond_signal(&cond);
                        }

                        size_t getFillState()
                        {
                                uint64_t f;

                                {
                                        MutexLock M(mutex);
                                        f = Q.size();
                                }

                                return f;
                        }

                        bool isTerminated()
                        {
                                bool lterminated;
                                {
                                MutexLock M(mutex);
                                lterminated = terminated;
                                }
                                return lterminated;
                        }

                        void terminate()
                        {
                                size_t numnoti = 0;
                                {
                                        MutexLock M(mutex);
                                        terminated = true;
                                        numnoti = numwait;
                                }
                                for ( size_t i = 0; i < numnoti; ++i )
                                        pthread_cond_signal(&cond);
                        }

                        value_type deque()
                        {
                                MutexLock M(mutex);

                                while ( (! terminated) && (!Q.size()) )
                                {
                                        numwait++;
                                        int const r = pthread_cond_wait(&cond,&mutex);
                                        if ( r != 0 )
                                        {
                                                int const error = errno;
                                                libmaus2::exception::LibMausException lme;
                                                lme.getStream() << "TerminatableSynchronousQueue::deque: pthread_cond_wait " << strerror(error) << std::endl;
                                                lme.finish();
                                                throw lme;
                                        }
                                        numwait--;
                                }

                                if ( Q.size() )
                                {
                                        value_type v = Q.front();
                                        Q.pop_front();
                                        return v;
                                }
                                else
                                {
                                        throw std::runtime_error("Queue is terminated");
                                }
                        }
                };
        }
}
#endif
#endif
