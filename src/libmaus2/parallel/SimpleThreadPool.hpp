/*
    libmaus2
    Copyright (C) 2009-2014 German Tischler
    Copyright (C) 2011-2014 Genome Research Limited

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
#if ! defined(LIBMAUS2_PARALLEL_SIMPLETHREADPOOL_HPP)
#define LIBMAUS2_PARALLEL_SIMPLETHREADPOOL_HPP

#include <libmaus2/parallel/PosixSpinLock.hpp>
#include <libmaus2/parallel/PosixConditionSemaphore.hpp>
#include <libmaus2/parallel/TerminatableSynchronousHeap.hpp>
#include <libmaus2/parallel/SimpleThreadPoolThread.hpp>
#include <libmaus2/parallel/SimpleThreadPoolInterface.hpp>
#include <libmaus2/parallel/SimpleThreadWorkPackageDispatcher.hpp>
#include <libmaus2/parallel/SimpleThreadWorkPackageComparator.hpp>
#include <libmaus2/parallel/SynchronousCounter.hpp>
#include <libmaus2/parallel/LockedBool.hpp>
#include <libmaus2/util/unordered_map.hpp>

#if defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#endif

namespace libmaus2
{
	namespace parallel
	{
		struct SimpleThreadPool : public SimpleThreadPoolInterface
		{
			typedef SimpleThreadPool this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;

			libmaus2::parallel::SynchronousCounter<uint64_t> nextDispatcherId;

			libmaus2::parallel::SynchronousCounter<uint64_t> nextpackageid;
			bool volatile panicflag;
			libmaus2::parallel::PosixSpinLock panicflaglock;
			libmaus2::exception::LibMausException::unique_ptr_type lme;

			// semaphore for notifying about start completion
			libmaus2::parallel::PosixConditionSemaphore startsem;

			// threads
			libmaus2::autoarray::AutoArray<SimpleThreadPoolThread::unique_ptr_type> threads;
			// package heap
			libmaus2::parallel::TerminatableSynchronousHeap<
				libmaus2::parallel::SimpleThreadWorkPackage *,
				libmaus2::parallel::SimpleThreadWorkPackageComparator
			> Q;

			libmaus2::parallel::PosixSpinLock globallock;

			#if defined(__linux__)
			// map linux tid -> thread id in this pool
			std::map<uint64_t,uint64_t> pmap;
			#endif

			std::vector<std::string> logvector;
			uint64_t logvectorcur;
			libmaus2::parallel::PosixSpinLock logvectorlock;

			libmaus2::parallel::PosixSpinLock & getGlobalLock()
			{
				return globallock;
			}

			uint64_t getNumThreads() const
			{
				return threads.size();
			}

			std::string getPanicMessage()
			{
                        	libmaus2::parallel::ScopePosixSpinLock lpanicflaglock(panicflaglock);
                        	if ( lme )
                        		return lme->what();
				else
					return std::string();
			}

                        void panic(libmaus2::exception::LibMausException const & ex)
                        {
                        	libmaus2::parallel::ScopePosixSpinLock lpanicflaglock(panicflaglock);
                        	Q.terminate();
                        	panicflag = true;

                        	if ( ! lme.get() )
                        	{
                        		libmaus2::exception::LibMausException::unique_ptr_type tex(ex.uclone());
                        		lme = UNIQUE_PTR_MOVE(tex);
				}
                        }

                        void panic(std::exception const & ex)
                        {
                        	libmaus2::parallel::ScopePosixSpinLock lpanicflaglock(panicflaglock);
                        	Q.terminate();
                        	panicflag = true;

                        	if ( ! lme.get() )
                        	{
                        		libmaus2::exception::LibMausException::unique_ptr_type tlme(
                        			new libmaus2::exception::LibMausException
                        		);
                        		lme = UNIQUE_PTR_MOVE(tlme);
                        		lme->getStream() << ex.what();
                        		lme->finish();
                        	}
                        }

                        bool isInPanicMode()
                        {
                        	libmaus2::parallel::ScopePosixSpinLock lpanicflaglock(panicflaglock);
				return panicflag;
                        }

                        void printPendingHistogram(std::ostream & out)
			{
				std::vector<libmaus2::parallel::SimpleThreadWorkPackage *> pending =
					Q.pending();
				std::map<char const *, uint64_t> hist;
				for ( uint64_t i = 0; i < pending.size(); ++i )
					hist[pending[i]->getPackageName()]++;
				for ( std::map<char const *, uint64_t>::const_iterator ita = hist.begin();
					ita != hist.end(); ++ita )
				{
					out << "P\t" << ita->first << "\t" << ita->second << "\n";
				}
			}

			void printRunningHistogram(std::ostream & out)
			{
				std::vector<libmaus2::parallel::SimpleThreadWorkPackage *> running;
				for ( uint64_t i = 0; i < threads.size(); ++i )
				{
					libmaus2::parallel::SimpleThreadWorkPackage * pack =
						threads[i]->getCurrentPackage();
					if ( pack )
						running.push_back(pack);
				}
				std::map<char const *, uint64_t> hist;
				for ( uint64_t i = 0; i < running.size(); ++i )
					hist[running[i]->getPackageName()]++;
				for ( std::map<char const *, uint64_t>::const_iterator ita = hist.begin();
					ita != hist.end(); ++ita )
				{
					out << "R\t" << ita->first << "\t" << ita->second << "\n";
				}
			}

			void printStateHistogram(std::ostream & out)
			{
				printPendingHistogram(out);
				printRunningHistogram(out);
			}

			// dispatcher map
			libmaus2::util::unordered_map<uint64_t,SimpleThreadWorkPackageDispatcher *>::type dispatchers;
			libmaus2::parallel::PosixSpinLock dispatcherslock;

			SimpleThreadPool(
				uint64_t const rnumthreads
			)
			: nextpackageid(0), panicflag(false), threads(rnumthreads), logvector(64), logvectorcur(0)
			{
				for ( uint64_t i = 0; i < threads.size(); ++i )
				{
					SimpleThreadPoolThread::unique_ptr_type tptr(new SimpleThreadPoolThread(*this,i));
					threads[i] = UNIQUE_PTR_MOVE(tptr);
				}
				for ( uint64_t i = 0; i < threads.size(); ++i )
				{
					threads[i]->start();
					// wait until thread is running
					startsem.wait();
				}
			}

			void internalJoin()
			{
				for ( uint64_t i = 0; i < threads.size(); ++i )
					threads[i]->tryJoin();
			}
			~SimpleThreadPool()
			{
				internalJoin();

				for ( uint64_t i = 0; i < threads.size(); ++i )
					threads[i].reset();
			}

			void join()
			{
				internalJoin();

				if ( lme.get() )
					throw *lme;
			}

			void enque(SimpleThreadWorkPackage * P)
			{
				P->packageid = nextpackageid++;
				Q.enque(P);
			}

			void terminate()
			{
				Q.terminate();
			}
			void notifyThreadStart()
			{
				startsem.post();
			}
			SimpleThreadWorkPackage * getPackage()
			{
				return Q.deque();
			}
			SimpleThreadWorkPackageDispatcher * getDispatcher(libmaus2::parallel::SimpleThreadWorkPackage * P)
			{
				SimpleThreadWorkPackageDispatcher * R;
				{
					libmaus2::parallel::ScopePosixSpinLock slock(dispatcherslock);
					libmaus2::util::unordered_map<uint64_t,SimpleThreadWorkPackageDispatcher *>::type::iterator it =
						dispatchers.find(P->dispatcherid);
					assert ( it != dispatchers.end() );
					R = it->second;
				}
				return R;
			}
			void registerDispatcher(uint64_t const id, SimpleThreadWorkPackageDispatcher * D)
			{
				libmaus2::parallel::ScopePosixSpinLock slock(dispatcherslock);
				dispatchers[id] = D;
			}
			void removeDispatcher(uint64_t const id)
			{
				libmaus2::parallel::ScopePosixSpinLock slock(dispatcherslock);
				libmaus2::util::unordered_map<uint64_t,SimpleThreadWorkPackageDispatcher *>::type::iterator it =
					dispatchers.find(id);
				assert ( it != dispatchers.end() );
				dispatchers.erase(id);
			}
			uint64_t getNextDispatcherId()
			{
				libmaus2::parallel::ScopePosixSpinLock slock(dispatcherslock);
				while ( dispatchers.find(nextDispatcherId) != dispatchers.end() )
					++nextDispatcherId;
				return nextDispatcherId++;
			}

			#if defined(__linux__)
			void setTaskId(uint64_t const threadid, uint64_t const taskid)
			{
				pmap[taskid] = threadid;
			}
			#endif

			virtual uint64_t getThreadId()
			{
				#if defined(__linux__)
				long const tid = syscall(SYS_gettid);
				return pmap.find(tid)->second;
				#else
				for ( uint64_t i = 0; i < threads.size(); ++i )
					if ( threads[i]->isCurrent() )
						return i;

				libmaus2::exception::LibMausException lme;
				lme.getStream() << "SimpleThreadPool::getThreadId(): unable to find thread\n";
				lme.finish();
				throw lme;
				#endif
			}

			virtual void addLogString(std::string const & s)
			{
				libmaus2::parallel::ScopePosixSpinLock slock(logvectorlock);
				logvector[(logvectorcur++)%logvector.size()] = s;
			}

			uint64_t rdtsc(void)
			{
				#if defined(LIBMAUS2_USE_ASSEMBLY) && defined(LIBMAUS2_HAVE_x86_64)
				uint32_t a, d;

				__asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

			        return static_cast<uint64_t>(a) | (static_cast<uint64_t>(d)<<32);
			        #else
			        return 0;
			        #endif
			}

			virtual void addLogStringWithThreadId(std::string const & s)
			{
				std::ostringstream ostr;
				ostr << "[" << getThreadId() << "," << rdtsc() << "] " << s;
				addLogString(ostr.str());
			}
			virtual void printLog(std::ostream & out = std::cerr)
			{
				libmaus2::parallel::ScopePosixSpinLock slock(logvectorlock);

				uint64_t const numlogentries = logvectorcur;
				uint64_t const numlogentriestoprint = std::min(numlogentries,static_cast<uint64_t>(logvector.size()));

				for ( uint64_t i = 0; i < numlogentriestoprint; ++i )
				{
					uint64_t const id = (logvectorcur + logvector.size() - numlogentriestoprint + i) % logvector.size();
					out << logvector[id] << '\n';
				}
			}
		};
	}
}
#endif
