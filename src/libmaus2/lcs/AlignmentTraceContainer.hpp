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

#if ! defined(LIBMAUS2_LCS_ALIGNMENTTRACECONTAINER_HPP)
#define LIBMAUS2_LCS_ALIGNMENTTRACECONTAINER_HPP

#include <libmaus2/lcs/PenaltyConstants.hpp>
#include <libmaus2/autoarray/AutoArray.hpp>
#include <libmaus2/lcs/AlignmentStatistics.hpp>
#include <libmaus2/math/lowbits.hpp>
#include <libmaus2/rank/popcnt.hpp>
#include <libmaus2/math/numbits.hpp>

#include <set>
#include <map>

namespace libmaus2
{
	namespace lcs
	{
		struct AlignmentTraceContainer : public PenaltyConstants
		{
			typedef AlignmentTraceContainer this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;

			private:
			AlignmentTraceContainer & operator=(AlignmentTraceContainer const &);
			AlignmentTraceContainer(AlignmentTraceContainer const &);

			public:
			virtual ~AlignmentTraceContainer() {}

			// trace
			::libmaus2::autoarray::AutoArray<step_type> trace;
			//
			step_type * te;
			step_type * ta;

			AlignmentTraceContainer(uint64_t const tracelen = 0)
			: trace(tracelen), te(trace.end()), ta(te)
			{

			}

			void swap(AlignmentTraceContainer & O)
			{
				trace.swap(O.trace);
				std::swap(ta,O.ta);
				std::swap(te,O.te);
			}

			static std::vector<uint64_t> periods(step_type const * ta, step_type const * te, uint64_t first)
			{
				std::vector<uint64_t> V;

				while ( ta != te )
				{
					V.push_back(first);

					uint64_t mat = 0, mis = 0, ins = 0, del = 0;

					while ( ta != te && mat+mis+del < first )
					{
						switch ( *(ta++) )
						{
							case STEP_MATCH:
								mat += 1;
								break;
							case STEP_MISMATCH:
								mis += 1;
								break;
							case STEP_DEL:
								del += 1;
								break;
							case STEP_INS:
								ins += 1;
								break;
							default:
								break;
						}
					}

					first = mat + mis + ins;
				}

				return V;
			}

			std::vector<uint64_t> periods(uint64_t first) const
			{
				return periods(ta,te,first);
			}

			std::vector < std::pair<step_type,uint64_t> > getOpBlocks() const
			{
				std::vector < std::pair<step_type,uint64_t> > R;

				step_type const * tc = ta;

				while ( tc != te )
				{
					step_type const * tt = tc;
					while ( tt != te && *tt == *tc )
						++tt;

					R.push_back(std::pair<step_type,uint64_t>(*tc,tt-tc));

					tc = tt;
				}

				return R;
			}

			size_t getOpBlocks(libmaus2::autoarray::AutoArray<std::pair<step_type,uint64_t> > & A) const
			{
				if ( static_cast<ptrdiff_t>(A.size()) < te-ta )
					A.resize(te-ta);

				step_type const * tc = ta;
				std::pair<step_type,uint64_t> * p = A.begin();

				while ( tc != te )
				{
					step_type const * tt = tc;
					while ( tt != te && *tt == *tc )
						++tt;

					*(p++) = std::pair<step_type,uint64_t>(*tc,tt-tc);

					tc = tt;
				}

				return p-A.begin();
			}

			static std::pair<step_type const *, step_type const *> getMaxMatch(step_type const * ta, step_type const * te)
			{
				step_type const * tc = ta;
				std::pair<step_type const *, step_type const *> I(te,te);

				while ( tc != te )
				{
					while ( tc != te && *tc != STEP_MATCH )
						++tc;

					step_type const * tx = tc;

					while ( tx != te && *tx == STEP_MATCH )
						++tx;

					if ( tx-tc > I.second-I.first )
					{
						I.first = tc;
						I.second = tx;
					}

					tc = tx;
				}

				return I;
			}

			std::pair<step_type const *, step_type const *> getMaxMatch() const
			{
				return getMaxMatch(ta,te);
			}

			static std::pair<uint64_t,uint64_t> getMaxMatchOffset(step_type const * ta, step_type const * te)
			{
				std::pair<step_type const *, step_type const *> P = getMaxMatch(ta,te);
				return getStringLengthUsed(ta,P.first);
			}

			std::pair<uint64_t,uint64_t> getMaxMatchOffset() const
			{
				return getMaxMatchOffset(ta,te);
			}

			void reset()
			{
				ta = te = trace.end();
			}

			void reverse()
			{
				std::reverse(ta,te);
			}

			unsigned int clipOffLastKMatches(unsigned int k)
			{
				unsigned int c = 0;
				while ( k-- && ta != te && te[-1] == STEP_MATCH )
				{
					--te;
					++c;
				}

				return c;
			}

			uint64_t windowError(unsigned int k = 8*sizeof(uint64_t)) const
			{
				unsigned int maxerr = 0;
				uint64_t evec = 0;
				uint64_t const emask = ::libmaus2::math::lowbits(k);

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					evec <<= 1;

					switch ( *tc )
					{
						case STEP_MISMATCH:
						case STEP_INS:
						case STEP_DEL:
							evec |= 1;
							break;
						case STEP_MATCH:
							evec |= 0;
							break;
						case STEP_RESET:
							evec = 0;
							break;
					}

					evec &= emask;

					maxerr = std::max(maxerr,static_cast<unsigned int>(libmaus2::rank::PopCnt8<sizeof(unsigned long)>::popcnt8(evec)));
				}

				return maxerr;
			}

			static uint64_t lastGoodWindow(step_type const * ta, step_type const * te, uint64_t const w, double const derr)
			{
				uint64_t const me = static_cast<uint64_t>(derr * w + 0.5);

				uint64_t c = 0;
				uint64_t e = 0;

				step_type const * t0 = ta;
				step_type const * t1 = ta;

				// accumulate positions until we reach w
				while ( t1 != te && c < w )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}
				}

				// trace is too short for a full window
				if ( c < w )
				{
					double const lerr = static_cast<double>(e) / static_cast<double>(c);

					if ( lerr <= derr )
						return t1-ta;
					else
						return ta-ta;
				}

				step_type const * lastw = ta;

				if ( e <= me )
					lastw = t1;

				// add more trace points
				while ( t1 != te )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}

					// remove from front of window until we reach w
					while ( t0 != t1 && c > w )
					{
						switch ( *t0++ )
						{
							case STEP_DEL:
							case STEP_MISMATCH:
								--c;
								--e;
								break;
							case STEP_MATCH:
								--c;
								break;
							case STEP_INS:
								--e;
								break;
							default:
								break;
						}
					}
					assert ( c == w );

					if ( e <= me )
						lastw = t1;
				}

				return lastw-ta;
			}


			struct WindowErrorLargeResult
			{
				double maxerr;
				step_type const * t0;
				step_type const * t1;

				WindowErrorLargeResult() {}
				WindowErrorLargeResult(double const rmaxerr, step_type const * rt0, step_type const * rt1)
				: maxerr(rmaxerr), t0(rt0), t1(rt1) {}
			};

			static WindowErrorLargeResult windowErrorLargeDetail(step_type const * ta, step_type const * te, uint64_t const w)
			{
				uint64_t c = 0;
				uint64_t e = 0;

				step_type const * t0 = ta;
				step_type const * t1 = ta;

				// accumulate positions until we reach w
				while ( t1 != te && c < w )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}
				}

				// set first window error
				double maxerr = e / static_cast<double>(c);
				step_type const * maxt0 = t0;
				step_type const * maxt1 = t1;

				// add more trace points
				while ( t1 != te )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}

					// remove from front of window until we reach w
					while ( t0 != t1 && c > w )
					{
						switch ( *t0++ )
						{
							case STEP_DEL:
							case STEP_MISMATCH:
								--c;
								--e;
								break;
							case STEP_MATCH:
								--c;
								break;
							case STEP_INS:
								--e;
								break;
							default:
								break;
						}
					}
					assert ( c == w );

					// set new maximum
					double const ed = e / static_cast<double>(c);
					if ( ed > maxerr )
					{
						maxerr = ed;
						maxt0 = t0;
						maxt1 = t1;
					}
				}

				return WindowErrorLargeResult(maxerr,maxt0,maxt1);
			}

			static double windowErrorLarge(step_type const * ta, step_type const * te, uint64_t const w)
			{
				uint64_t c = 0;
				uint64_t e = 0;

				step_type const * t0 = ta;
				step_type const * t1 = ta;

				// accumulate positions until we reach w
				while ( t1 != te && c < w )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}
				}

				// set first window error
				double maxerr = e / static_cast<double>(c);

				// add more trace points
				while ( t1 != te )
				{
					switch ( *t1++ )
					{
						case STEP_DEL:
						case STEP_MISMATCH:
							c++;
							e++;
							break;
						case STEP_MATCH:
							c++;
							break;
						case STEP_INS:
							e++;
							break;
						default:
							break;
					}

					// remove from front of window until we reach w
					while ( t0 != t1 && c > w )
					{
						switch ( *t0++ )
						{
							case STEP_DEL:
							case STEP_MISMATCH:
								--c;
								--e;
								break;
							case STEP_MATCH:
								--c;
								break;
							case STEP_INS:
								--e;
								break;
							default:
								break;
						}
					}
					assert ( c == w );

					// set new maximum
					double const ed = e / static_cast<double>(c);
					if ( ed > maxerr )
						maxerr = ed;
				}

				return maxerr;
			}

			double windowErrorLarge(uint64_t const w) const
			{
				return windowErrorLarge(ta,te,w);
			}

			struct ClipPair
			{
				std::pair<uint64_t,uint64_t> A;
				std::pair<uint64_t,uint64_t> B;

				ClipPair() {}
				ClipPair(
					std::pair<uint64_t,uint64_t> const & rA,
					std::pair<uint64_t,uint64_t> const & rB
				) : A(rA), B(rB) {}

				bool check() const
				{
					return
						A.first < A.second &&
						B.first < B.second;
				}

				static ClipPair merge(ClipPair const & A, ClipPair const & B)
				{
					return
						ClipPair(
							std::pair<uint64_t,uint64_t>(
								A.A.first,
								B.A.second
							),
							std::pair<uint64_t,uint64_t>(
								A.B.first,
								B.B.second
							)
						);
				}

				static bool overlap(ClipPair const & A, ClipPair const & B)
				{
					if ( A.A.first > B.A.first )
						return overlap(B,A);

					assert ( A.A.first <= B.A.first );
					assert ( A.check() );
					assert ( B.check() );

					bool oa = A.A.second >= B.A.first;
					bool ob = A.B.second >= B.B.first;

					return oa || ob;
				}
			};

			static bool transitiveCross(
				AlignmentTraceContainer const & AB,
				size_t ABapos,
				size_t ABbpos,
				AlignmentTraceContainer const & BC,
				size_t BCbpos,
				size_t BCcpos,
				AlignmentTraceContainer const & AC,
				size_t ACapos,
				size_t ACcpos
			)
			{
				step_type const * ABtc = AB.ta;
				step_type const * BCtc = BC.ta;
				step_type const * ACtc = AC.ta;

				#if 0
				std::cerr << std::string(80,'-') << std::endl;
				#endif

				while ( ABtc != AB.te && BCtc != BC.te && ACtc != AC.te )
				{
					#if 0
					std::cerr
						<< "ABapos=" << ABapos << " ABbpos=" << ABbpos
						<< "BCbpos=" << BCbpos << " BCcpos=" << BCcpos
						<< "ACapos=" << ACapos << " ACcpos=" << ACcpos
						<< std::endl;
					#endif

					if ( ABbpos < BCbpos )
					{
						switch ( *(ABtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								ABapos += 1;
								ABbpos += 1;
								break;
							case STEP_INS:
								ABbpos += 1;
								break;
							case STEP_DEL:
								ABapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if ( BCbpos < ABbpos )
					{
						switch ( *(BCtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								BCbpos += 1;
								BCcpos += 1;
								break;
							case STEP_INS:
								BCcpos += 1;
								break;
							case STEP_DEL:
								BCbpos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if (
						*ABtc != *BCtc
						||
						*ABtc != STEP_MATCH
						||
						ABapos < ACapos
						||
						BCcpos < ACcpos
					)
					{
						switch ( *(ABtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								ABapos += 1;
								ABbpos += 1;
								break;
							case STEP_INS:
								ABbpos += 1;
								break;
							case STEP_DEL:
								ABapos += 1;
								break;
							case STEP_RESET:
								break;
						}
						switch ( *(BCtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								BCbpos += 1;
								BCcpos += 1;
								break;
							case STEP_INS:
								BCcpos += 1;
								break;
							case STEP_DEL:
								BCbpos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if (
						ACapos < ABapos
						||
						ACcpos < BCcpos
						||
						*ACtc != STEP_MATCH
					)
					{
						switch ( *(ACtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								ACapos += 1;
								ACcpos += 1;
								break;
							case STEP_INS:
								ACcpos += 1;
								break;
							case STEP_DEL:
								ACapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else
					{
						assert ( ABbpos == BCbpos );
						assert ( *ABtc == *BCtc );
						assert ( *ABtc == STEP_MATCH );
						assert ( ACapos == ABapos );
						assert ( ACcpos == BCcpos );
						assert ( *ACtc == STEP_MATCH );

						return true;
					}
				}

				return false;
			}

			// A, B
			// C, A
			// C, B
			static bool tripleCross(
				AlignmentTraceContainer const & AB,
				size_t ABapos,
				size_t ABbpos,
				AlignmentTraceContainer const & CA,
				size_t CAcpos,
				size_t CAapos,
				AlignmentTraceContainer const & CB,
				size_t CBcpos,
				size_t CBbpos
			)
			{
				step_type const * ABtc = AB.ta;
				step_type const * CAtc = CA.ta;
				step_type const * CBtc = CB.ta;

				while ( ABtc != AB.te && CAtc != CA.te && CBtc != CB.te )
				{
					if ( CAcpos < CBcpos )
					{
						switch ( *(CAtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								CAcpos += 1;
								CAapos += 1;
								break;
							case STEP_INS:
								CAapos += 1;
								break;
							case STEP_DEL:
								CAcpos += 1;
								break;
							case STEP_RESET:
								break;

						}
					}
					else if ( CBcpos < CAcpos )
					{
						switch ( *(CBtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								CBcpos += 1;
								CBbpos += 1;
								break;
							case STEP_INS:
								CBbpos += 1;
								break;
							case STEP_DEL:
								CBcpos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if (
						*CAtc != *CBtc
						||
						*CAtc != STEP_MATCH
						||
						CAapos < ABapos
						||
						CBbpos < ABbpos
					)
					{
						switch ( *(CAtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								CAcpos += 1;
								CAapos += 1;
								break;
							case STEP_INS:
								CAapos += 1;
								break;
							case STEP_DEL:
								CAcpos += 1;
								break;
							case STEP_RESET:
								break;

						}
						switch ( *(CBtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								CBcpos += 1;
								CBbpos += 1;
								break;
							case STEP_INS:
								CBbpos += 1;
								break;
							case STEP_DEL:
								CBcpos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if ( ABapos < CAapos || ABbpos < CBbpos || *ABtc != STEP_MATCH )
					{
						switch ( *(ABtc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								ABapos += 1;
								ABbpos += 1;
								break;
							case STEP_INS:
								ABbpos += 1;
								break;
							case STEP_DEL:
								ABapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else
					{
						assert ( CAcpos == CBcpos );
						assert ( CAapos == ABapos );
						assert ( CBbpos == ABbpos );
						assert ( *CAtc == STEP_MATCH );
						assert ( *CBtc == STEP_MATCH );
						assert ( *ABtc == STEP_MATCH );

						return true;
					}
				}

				return false;
			}

			static bool cross(
				AlignmentTraceContainer const & A,
				size_t Aapos,
				size_t Abpos,
				uint64_t & offseta,
				AlignmentTraceContainer const & B,
				size_t Bapos,
				size_t Bbpos,
				uint64_t & offsetb
			)
			{
				step_type const * Atc = A.ta;
				step_type const * Btc = B.ta;

				while (
					(!(Aapos == Bapos && Abpos == Bbpos)) && (Atc != A.te) && (Btc != B.te)
				)
				{
					// std::cerr << Aapos << "," << Abpos << " " << Bapos << "," << Bbpos << std::endl;

					while (
						(Atc != A.te) &&
						(
							(Aapos < Bapos) ||
							((Aapos == Bapos) && (Abpos < Bbpos))
						)
					)
					{
						switch ( *(Atc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Aapos += 1;
								Abpos += 1;
								break;
							case STEP_INS:
								Abpos += 1;
								break;
							case STEP_DEL:
								Aapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					while (
						(Btc != B.te) &&
						(
							(Bapos < Aapos)
							||
							((Bapos == Aapos) && (Bbpos < Abpos))
						)
					)
					{
						switch ( *(Btc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bapos += 1;
								Bbpos += 1;
								break;
							case STEP_INS:
								Bbpos += 1;
								break;
							case STEP_DEL:
								Bapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
				}

				offseta = Atc - A.ta;
				offsetb = Btc - B.ta;

				return
					(Aapos == Bapos) && (Abpos == Bbpos);
			}

			static bool cross(step_type const * ta, step_type const * te, size_t apos, size_t bpos)
			{
				for ( ; ta != te; ++ta )
				{
					switch ( *ta )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							if ( apos == bpos )
							{
								assert ( *ta == STEP_MATCH );
								return true;
							}

							++apos, ++bpos;
							break;
						case STEP_INS:
							++bpos;
							break;
						case STEP_DEL:
							++apos;
							break;
						default:
							break;
					}
				}

				return false;
			}

			bool cross(size_t const apos, size_t const bpos) const
			{
				return cross(ta,te,apos,bpos);
			}

			static bool a_sync(
				AlignmentTraceContainer const & A,
				size_t Aapos,
				uint64_t & offseta,
				AlignmentTraceContainer const & B,
				size_t Bapos,
				uint64_t & offsetb
			)
			{
				step_type const * Atc = A.ta;
				step_type const * Btc = B.ta;

				while (
					(!(Aapos == Bapos)) && (Atc != A.te) && (Btc != B.te)
				)
				{
					// std::cerr << Aapos << "," << Abpos << " " << Bapos << "," << Bbpos << std::endl;

					while ( (Atc != A.te) && (Aapos < Bapos) )
					{
						switch ( *(Atc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Aapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Aapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					while ( (Btc != B.te) && (Bapos < Aapos) )
					{
						switch ( *(Btc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Bapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
				}

				offseta = Atc - A.ta;
				offsetb = Btc - B.ta;

				return (Aapos == Bapos);
			}

			static bool a_sync_match(
				AlignmentTraceContainer const & A,
				size_t Aapos,
				uint64_t & offseta,
				AlignmentTraceContainer const & B,
				size_t Bapos,
				uint64_t & offsetb
			)
			{
				step_type const * Atc = A.ta;
				step_type const * Btc = B.ta;

				while (
					(Atc != A.te) && (Btc != B.te)
				)
				{
					// std::cerr << Aapos << "," << Abpos << " " << Bapos << "," << Bbpos << std::endl;

					if ( (Aapos < Bapos) )
					{
						switch ( *(Atc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Aapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Aapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if ( (Bapos < Aapos) )
					{
						switch ( *(Btc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Bapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else if ( *Atc != STEP_MATCH || *Btc != STEP_MATCH )
					{
						switch ( *(Atc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Aapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Aapos += 1;
								break;
							case STEP_RESET:
								break;
						}
						switch ( *(Btc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bapos += 1;
								break;
							case STEP_INS:
								break;
							case STEP_DEL:
								Bapos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}
					else
					{
						assert ( Aapos == Bapos );
						assert ( *Atc == STEP_MATCH );
						assert ( *Btc == STEP_MATCH );

						offseta = Atc - A.ta;
						offsetb = Btc - B.ta;

						return true;
					}
				}

				return false;
			}

			static bool b_sync(
				AlignmentTraceContainer const & A,
				size_t Abpos,
				uint64_t & offseta,
				AlignmentTraceContainer const & B,
				size_t Bbpos,
				uint64_t & offsetb
			)
			{
				step_type const * Atc = A.ta;
				step_type const * Btc = B.ta;

				while (
					(!(Abpos == Bbpos)) && (Atc != A.te) && (Btc != B.te)
				)
				{
					// std::cerr << Aapos << "," << Abpos << " " << Bapos << "," << Bbpos << std::endl;

					while (
						(Atc != A.te) &&  (Abpos < Bbpos)
					)
					{
						switch ( *(Atc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Abpos += 1;
								break;
							case STEP_INS:
								Abpos += 1;
								break;
							case STEP_DEL:
								break;
							case STEP_RESET:
								break;
						}
					}
					while ( (Btc != B.te) && (Bbpos < Abpos) )
					{
						switch ( *(Btc++) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bbpos += 1;
								break;
							case STEP_INS:
								Bbpos += 1;
								break;
							case STEP_DEL:
								break;
							case STEP_RESET:
								break;
						}
					}
				}

				offseta = Atc - A.ta;
				offsetb = Btc - B.ta;

				return (Abpos == Bbpos);
			}

			static bool b_sync_reverse(
				AlignmentTraceContainer const & A,
				size_t Abpos,
				uint64_t & offseta,
				AlignmentTraceContainer const & B,
				size_t Bbpos,
				uint64_t & offsetb
			)
			{
				step_type const * Atc = A.te;
				step_type const * Btc = B.te;

				while ( (!(Abpos == Bbpos)) && (Atc != A.ta) && (Btc != B.ta) )
				{
					// std::cerr << Aapos << "," << Abpos << " " << Bapos << "," << Bbpos << std::endl;

					while (
						(Atc != A.ta) &&  (Abpos > Bbpos)
					)
					{
						switch ( *(--Atc) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Abpos -= 1;
								break;
							case STEP_INS:
								Abpos -= 1;
								break;
							case STEP_DEL:
								break;
							case STEP_RESET:
								break;
						}
					}
					while ( (Btc != B.ta) && (Bbpos > Abpos) )
					{
						switch ( *(--Btc) )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								Bbpos -= 1;
								break;
							case STEP_INS:
								Bbpos -= 1;
								break;
							case STEP_DEL:
								break;
							case STEP_RESET:
								break;
						}
					}
				}

				offseta = Atc - A.ta;
				offsetb = Btc - B.ta;

				return (Abpos == Bbpos);
			}

			static void getDiagonalBand(step_type * ta, step_type * te, int64_t & from, int64_t & to)
			{
				from = to = 0;
				int64_t cur = 0;

				while ( ta != te )
				{
					switch ( *(ta++) )
					{
						case STEP_INS:
							cur -= 1;
							break;
						case STEP_DEL:
							cur += 1;
							break;
						case STEP_MATCH:
							from = std::min(from,cur);
							to = std::max(to,cur);
							break;
						default:
							break;

					}
				}
			}

			std::pair<int64_t,int64_t> getDiagonalBand(int64_t const apos = 0, int64_t const bpos = 0) const
			{
				std::pair<int64_t,int64_t> P;
				int64_t const d = apos-bpos;
				getDiagonalBand(ta,te,P.first,P.second);
				P.first += d;
				P.second += d;
				return P;
			}

			static void swapRoles(step_type * ta, step_type * te)
			{
				for ( step_type * tc = ta; tc != te; ++tc )
					switch ( *tc )
					{
						case STEP_INS:
							*tc = STEP_DEL;
							break;
						case STEP_DEL:
							*tc = STEP_INS;
							break;
						default:
							break;
					}
			}

			void swapRoles()
			{
				swapRoles(ta,te);
			}

			std::vector < ClipPair > lowQuality(int const k, unsigned int const thres) const
			{
				std::vector < ClipPair > R;

				#if defined(LIBMAUS2_HAVE_UNSIGNED_INT128)
				typedef libmaus2::uint128_t mask_type;
				#else
				typedef uint64_t mask_type;
				#endif

				assert ( k <= static_cast<int>(sizeof(mask_type) * 8) );

				if ( k && (te-ta) >= k )
				{
					std::vector < std::pair<uint64_t,uint64_t> > V;

					mask_type const outmask = static_cast<mask_type>(1) << (k-1);
					mask_type w = 0;
					unsigned int e = 0;

					step_type const * tc = ta;

					for ( int i = 0; i < (k-1); ++i, ++tc )
						switch ( *tc )
						{
							case STEP_MISMATCH:
							case STEP_INS:
							case STEP_DEL:
								w <<= 1;
								w |= 1;
								e += 1;
								break;
							case STEP_MATCH:
								w <<= 1;
								break;
							case STEP_RESET:
								break;
						}

					int64_t low = -1, high = -1;

					while ( tc != te )
					{
						if ( w & outmask )
							e -= 1;

						w <<= 1;
						switch ( *(tc++) )
						{
							case STEP_MISMATCH:
							case STEP_INS:
							case STEP_DEL:
								w |= 1;
								e += 1;
								break;
							case STEP_MATCH:
								break;
							case STEP_RESET:
								break;
						}

						if ( e >= thres )
						{
							if ( low < 0 )
							{
								low = (tc-ta)-k;
								high = tc-ta;
							}
							else
							{
								high = tc-ta;
							}
						}
						else
						{
							if ( low >= 0 )
								V.push_back(std::pair<uint64_t,uint64_t>(low,high));
							low = high = -1;
						}
					}

					if ( low >= 0 )
						V.push_back(std::pair<uint64_t,uint64_t>(low,high));

					std::map<uint64_t, std::pair<uint64_t,uint64_t> > pmap;
					std::set<uint64_t> pset;
					for ( uint64_t i = 0; i < V.size(); ++i )
					{
						pset.insert(V[i].first);
						pset.insert(V[i].second);
					}

					uint64_t apos = 0, bpos = 0;
					for ( tc = ta; tc != te; ++tc )
					{
						if ( pset.find(tc-ta) != pset.end() )
							pmap[tc-ta] = std::pair<uint64_t,uint64_t>(apos,bpos);

						switch ( *tc )
						{
							case STEP_MATCH:
							case STEP_MISMATCH:
								apos += 1;
								bpos += 1;
								break;
							case STEP_INS:
								bpos += 1;
								break;
							case STEP_DEL:
								apos += 1;
								break;
							case STEP_RESET:
								break;
						}
					}

					if ( pset.find(tc-ta) != pset.end() )
						pmap[tc-ta] = std::pair<uint64_t,uint64_t>(apos,bpos);

					for ( uint64_t i = 0; i < V.size(); ++i )
					{
						assert ( pmap.find(V[i].first) != pmap.end() );
						assert ( pmap.find(V[i].second) != pmap.end() );

						std::map<uint64_t, std::pair<uint64_t,uint64_t> >::const_iterator Vfirst = pmap.find(V[i].first);
						std::map<uint64_t, std::pair<uint64_t,uint64_t> >::const_iterator Vsecond = pmap.find(V[i].second);

						std::pair<uint64_t,uint64_t> apos(Vfirst->second.first ,Vsecond->second.first);
						std::pair<uint64_t,uint64_t> bpos(Vfirst->second.second,Vsecond->second.second);

						R.push_back(ClipPair(apos,bpos));
					}
				}

				uint64_t low = 0;
				uint64_t o = 0;

				while ( low < R.size() )
				{
					ClipPair CP = R[low];

					uint64_t high = low+1;
					while ( high < R.size() && ClipPair::overlap(CP,R[high]) )
					{
						CP = ClipPair::merge(CP,R[high]);
						++high;
					}

					R[o++] = CP;

					low = high;
				}

				R.resize(o);

				return R;
			}

			std::pair<uint64_t,uint64_t> prefixPositive(
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			)
			{
				std::reverse(ta,te);
				std::pair<uint64_t,uint64_t> const P = suffixPositive(match_score,mismatch_score,ins_score,del_score);
				std::reverse(ta,te);
				return P;
			}

			static int64_t getScore(
				step_type const * ta,
				step_type const * te,
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			)
			{
				int64_t score = 0;

				while ( ta != te )
				{
					step_type const cur = *(ta++);

					switch ( cur )
					{
						case STEP_MATCH:
							score += match_score;
							break;
						case STEP_MISMATCH:
							score -= mismatch_score;
							break;
						case STEP_INS:
							score -= ins_score;
							break;
						case STEP_DEL:
							score -= del_score;
							break;
						case STEP_RESET:
							score = 0;
							break;
					}
				}

				return score;
			}

			std::pair<step_type const *, step_type const *> bestLocalScore(
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			) const
			{
				return bestLocalScore(ta,te,match_score,mismatch_score,ins_score,del_score);
			}

			static std::pair<step_type const *, step_type const *> bestLocalScore(
				step_type const * ta,
				step_type const * te,
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			)
			{
				step_type const * tc = ta;
				step_type const * tbegin = ta;

				step_type const * tbeststart = ta;
				step_type const * tbestend = ta;

				int64_t score = 0;
				int64_t bestscore = 0;

				while ( tc != te )
				{
					step_type const cur = *(tc++);

					switch ( cur )
					{
						case STEP_MATCH:
							score += match_score;
							break;
						case STEP_MISMATCH:
							score -= mismatch_score;
							break;
						case STEP_INS:
							score -= ins_score;
							break;
						case STEP_DEL:
							score -= del_score;
							break;
						case STEP_RESET:
							score = 0;
							break;
					}

					if ( score > bestscore )
					{
						bestscore = score;
						tbeststart = tbegin;
						tbestend = tc;
					}
					if ( score < 0 )
					{
						score = 0;
						tbegin = tc;
					}
				}

				return std::pair<step_type const *, step_type const *>(tbeststart,tbestend);
			}

			std::pair<step_type const *, step_type const *> bLengthFront(uint64_t l)
			{
				step_type const * tc = ta;

				while ( tc != te && l )
				{
					switch ( *(tc++) )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							--l;
							break;
						case STEP_DEL:
							break;
						case STEP_INS:
							--l;
							break;
						case STEP_RESET:
							break;
					}
				}

				assert ( ! l );

				return std::pair<step_type const *, step_type const *>(ta,tc);
			}

			std::pair<step_type const *, step_type const *> bLengthBack(uint64_t l)
			{
				step_type const * tc = te;

				while ( tc != ta && l )
				{
					switch ( *(--tc) )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							--l;
							break;
						case STEP_DEL:
							break;
						case STEP_INS:
							--l;
							break;
						case STEP_RESET:
							break;
					}
				}

				assert ( ! l );

				return std::pair<step_type const *, step_type const *>(tc,te);
			}

			template<typename step_type_in_ptr>
			static std::pair<uint64_t,uint64_t> suffixPositive(
				step_type_in_ptr & ta,
				step_type_in_ptr & te,
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			)
			{
				step_type_in_ptr tc = te;
				step_type_in_ptr tne = te;

				int64_t score = 0;
				while ( tc != ta )
				{
					step_type const cur = *(--tc);

					switch ( cur )
					{
						case STEP_MATCH:
							score += match_score;
							break;
						case STEP_MISMATCH:
							score -= mismatch_score;
							break;
						case STEP_INS:
							score -= ins_score;
							break;
						case STEP_DEL:
							score -= del_score;
							break;
						case STEP_RESET:
							score = 0;
							break;
					}

					if ( score <= 0 )
					{
						score = 0;
						tne = tc;
					}
				}

				// count how many symbols need to be removed from the back of a and b resp
				uint64_t rema = 0, remb = 0;
				for ( tc = tne; tc != te; ++tc )
					switch ( *tc )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							rema += 1;
							remb += 1;
							break;
						case STEP_DEL:
							rema += 1;
							break;
						case STEP_INS:
							remb += 1;
							break;
						case STEP_RESET:
							break;
					}

				te = tne;

				return std::pair<uint64_t,uint64_t>(rema,remb);
			}

			std::pair<uint64_t,uint64_t> suffixPositive(
				int64_t const match_score    = gain_match,
				int64_t const mismatch_score = penalty_subst,
				int64_t const ins_score      = penalty_ins,
				int64_t const del_score      = penalty_del
			)
			{
				return suffixPositive(ta,te,match_score,mismatch_score,ins_score,del_score);
			}


			void push(AlignmentTraceContainer const & O)
			{
				size_t const pre = getTraceLength();
				size_t const oth = O.getTraceLength();

				if ( ta != trace.begin() )
				{
					std::copy(ta,te,trace.begin());
					ta = trace.begin();
					te = trace.begin()+pre;
				}

				if ( pre + oth > trace.size() )
				{
					trace.resize(libmaus2::math::nextTwoPow(pre + oth));

					ta = trace.begin();
					te = trace.begin()+pre;
				}

				for ( size_t i = 0; i < oth; ++i )
					*(te++) = O.ta[i];
			}

			void resize(uint64_t const tracelen)
			{
				trace = ::libmaus2::autoarray::AutoArray<step_type>(tracelen,false);
				te = trace.end();
				ta = te;
			}

			uint64_t capacity() const
			{
				return trace.size();
			}

			uint64_t getTraceLength() const
			{
				return te-ta;
			}

			template<typename it>
			static int32_t getTraceScore(it ta, it te)
			{
				int32_t score = 0;
				for ( it tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							score += gain_match;
							break;
						case STEP_MISMATCH:
							score -= penalty_subst;
							break;
						case STEP_INS:
							score -= penalty_ins;
							break;
						case STEP_DEL:
							score -= penalty_del;
							break;
						case STEP_RESET:
							score = 0;
							break;
					}
				}
				return score;
			}

			static std::vector< std::pair<int64_t,uint64_t> > matchDiagonalHistogram(step_type const * ta, step_type const * te)
			{
				std::map<int64_t,uint64_t> M;
				uint64_t apos = 0, bpos = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							M [ static_cast<int64_t>(apos)-static_cast<int64_t>(bpos) ] += 1;
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				std::vector< std::pair<int64_t,uint64_t> > V;
				for ( std::map<int64_t,uint64_t>::const_iterator ita = M.begin(); ita != M.end(); ++ita )
					V.push_back(std::pair<int64_t,uint64_t>(ita->first,ita->second));

				return V;
			}

			// compute how many operations we consume when going adv steps forward on a
			static std::pair<uint64_t,uint64_t> advanceA(step_type const * ta, step_type const * te, uint64_t const adv)
			{
				uint64_t apos = 0, bpos = 0;
				step_type const * tc = ta;

				for ( ; apos < adv && tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return std::pair<uint64_t,uint64_t>(apos,tc-ta);
			}

			std::pair<uint64_t,uint64_t> advanceA(uint64_t const adv) const
			{
				return advanceA(ta,te,adv);
			}

			// compute how many operations we consume when going adv steps forward on b
			static std::pair<uint64_t,uint64_t> advanceB(step_type const * ta, step_type const * te, uint64_t const adv)
			{
				uint64_t apos = 0, bpos = 0;
				step_type const * tc = ta;

				for ( ; bpos < adv && tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return std::pair<uint64_t,uint64_t>(bpos,tc-ta);
			}


			// compute how many operations we consume when going adv steps forward on b
			static std::pair<uint64_t,uint64_t> advanceMaxB(step_type const * ta, step_type const * te, uint64_t const adv)
			{
				uint64_t apos = 0, bpos = 0;
				step_type const * tc = ta;

				for ( ; bpos < adv && tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				// can me move further in the trace without incrementing bpos?
				while ( bpos == adv && tc != te && *tc == STEP_DEL )
				{
					apos++;
					tc++;
				}

				return std::pair<uint64_t,uint64_t>(bpos,tc-ta);
			}

			std::pair<uint64_t,uint64_t> advanceB(uint64_t const adv) const
			{
				return advanceB(ta,te,adv);
			}

			std::vector< std::pair<int64_t,uint64_t> > matchDiagonalHistogram()
			{
				return matchDiagonalHistogram(ta,te);
			}

			static AlignmentStatistics getAlignmentStatistics(step_type const * ta, step_type const * te)
			{
				AlignmentStatistics stats;

				for ( step_type const * tc = ta; tc != te; ++tc )
					switch ( *tc )
					{
						case STEP_MATCH:
							stats.matches++;
							break;
						case STEP_MISMATCH:
							stats.mismatches++;
							break;
						case STEP_INS:
							stats.insertions++;
							break;
						case STEP_DEL:
							stats.deletions++;
							break;
						case STEP_RESET:
							break;
					}

				return stats;
			}

			AlignmentStatistics getAlignmentStatistics() const
			{
				return getAlignmentStatistics(ta,te);
			}

			static uint64_t getNumErrors(step_type const * ta, step_type const * te)
			{
				AlignmentStatistics stats = getAlignmentStatistics(ta,te);
				return stats.mismatches + stats.insertions + stats.deletions;
			}

			uint64_t getNumErrors() const
			{
				return getNumErrors(ta,te);
			}

			std::pair<uint64_t,uint64_t> getStringLengthUsed() const
			{
				return getStringLengthUsed(ta,te);
			}

			static std::pair<uint64_t,uint64_t> getStringLengthUsed(step_type const * ta, step_type const * te)
			{
				uint64_t apos = 0, bpos = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return std::pair<uint64_t,uint64_t>(apos,bpos);
			}

			template<typename iterator>
			static bool checkAlignment(step_type const * ta, step_type const * te, iterator a, iterator b)
			{
				bool ok = true;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							ok = ok && (*a == *b);
							a += 1;
							b += 1;
							break;
						case STEP_MISMATCH:
							ok = ok && (*a != *b);
							a += 1;
							b += 1;
							break;
						case STEP_INS:
							b += 1;
							break;
						case STEP_DEL:
							a += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return ok;
			}

			std::vector < std::pair<uint64_t,uint64_t> > getTracePoints() const
			{
				uint64_t apos = 0, bpos = 0;
				std::vector < std::pair<uint64_t,uint64_t> > R;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					R.push_back(std::pair<uint64_t,uint64_t>(apos,bpos));
					switch ( *tc )
					{
						case STEP_MATCH:
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				R.push_back(std::pair<uint64_t,uint64_t>(apos,bpos));

				return R;
			}

			std::vector < std::pair<uint64_t,uint64_t> >
				getKMatchOffsets(unsigned int const k, uint64_t const off_a = 0, uint64_t const off_b = 0) const
			{
				std::vector < std::pair<uint64_t,uint64_t> > R;
				getKMatchOffsets(k,R,off_a,off_b);
				return R;
			}

			static void getKMatchOffsets(step_type const * ta, step_type const * te, unsigned int const k, std::vector < std::pair<uint64_t,uint64_t> > & R,  uint64_t const off_a = 0, uint64_t const off_b = 0)
			{
				uint64_t apos = 0, bpos = 0;
				uint64_t const kmask = libmaus2::math::lowbits(k);
				uint64_t const kmask1 = k ? libmaus2::math::lowbits(k-1) : 0;
				uint64_t e = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					if ( (tc-ta >= static_cast<ptrdiff_t>(k)) && (e == kmask) )
					{
						assert ( apos >= k );
						assert ( bpos >= k );
						R.push_back(std::pair<uint64_t,uint64_t>(apos-k+off_a,bpos-k+off_b));
					}

					e &= kmask1;
					e <<= 1;

					switch ( *tc )
					{
						case STEP_MATCH:
							apos += 1;
							bpos += 1;
							e |= 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				if ( (te-ta >= static_cast<ptrdiff_t>(k)) && (e == kmask) )
				{
					assert ( apos >= k );
					assert ( bpos >= k );
					R.push_back(std::pair<uint64_t,uint64_t>(apos-k+off_a,bpos-k+off_b));
				}
			}

			struct KMatch
			{
				uint64_t a;
				uint64_t b;

				KMatch(uint64_t const ra = 0, uint64_t const rb = 0)
				: a(ra), b(rb)
				{

				}

				int64_t getBand() const
				{
					return static_cast<int64_t>(a)-static_cast<int64_t>(b);
				}
			};

			struct KMatchBandComparator
			{
				bool operator()(KMatch const & A, KMatch const & B) const
				{
					return A.getBand() < B.getBand();
				}
			};

			static void getKMatchOffsets(step_type const * ta, step_type const * te, unsigned int const k, std::vector < KMatch > & R,  uint64_t const off_a = 0, uint64_t const off_b = 0)
			{
				uint64_t apos = 0, bpos = 0;
				uint64_t const kmask = libmaus2::math::lowbits(k);
				uint64_t const kmask1 = k ? libmaus2::math::lowbits(k-1) : 0;
				uint64_t e = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					if ( (tc-ta >= static_cast<ptrdiff_t>(k)) && (e == kmask) )
					{
						assert ( apos >= k );
						assert ( bpos >= k );
						R.push_back(KMatch(apos-k+off_a,bpos-k+off_b));
					}

					e &= kmask1;
					e <<= 1;

					switch ( *tc )
					{
						case STEP_MATCH:
							apos += 1;
							bpos += 1;
							e |= 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				if ( (te-ta >= static_cast<ptrdiff_t>(k)) && (e == kmask) )
				{
					assert ( apos >= k );
					assert ( bpos >= k );
					R.push_back(KMatch(apos-k+off_a,bpos-k+off_b));
				}
			}

			static void getMatchOffsets(step_type const * ta, step_type const * te, std::vector < std::pair<uint64_t,uint64_t> > & R,  uint64_t const off_a = 0, uint64_t const off_b = 0)
			{
				uint64_t apos = off_a, bpos = off_b;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							R.push_back(std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}
			}

			static uint64_t getAOffsets(
				step_type const * ta,
				step_type const * te,
				libmaus2::autoarray::AutoArray < std::pair<uint64_t,uint64_t> > & R,
				uint64_t const off_a = 0, uint64_t const off_b = 0,
				uint64_t o = 0
			)
			{
				uint64_t apos = off_a, bpos = off_b;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return o;
			}

			static uint64_t getBOffsets(
				step_type const * ta,
				step_type const * te,
				libmaus2::autoarray::AutoArray < std::pair<uint64_t,uint64_t> > & R,
				uint64_t const off_a = 0, uint64_t const off_b = 0,
				uint64_t o = 0
			)
			{
				uint64_t apos = off_a, bpos = off_b;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							R.push(o,std::pair<uint64_t,uint64_t>(apos,bpos));
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				return o;
			}

			struct Match
			{
				uint64_t apos;
				uint64_t bpos;
				uint64_t length;

				Match(
					uint64_t const rapos = 0,
					uint64_t const rbpos = 0,
					uint64_t const rlength = 0
				) : apos(rapos), bpos(rbpos), length(rlength) {}

				int64_t getDiagonal() const
				{
					return static_cast<int64_t>(apos) - static_cast<int64_t>(bpos);
				}

				uint64_t getAntiDiagonal() const
				{
					return apos + bpos;
				}
			};

			static void getMatchOffsets(
				step_type const * ta,
				step_type const * te,
				std::vector < Match > & R,
				uint64_t const off_a = 0, uint64_t const off_b = 0
			)
			{
				uint64_t apos = off_a, bpos = off_b;
				uint64_t matchlen = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MISMATCH:
						case STEP_INS:
						case STEP_DEL:
							if ( matchlen )
							{
								R.push_back(Match(apos-matchlen,bpos-matchlen,matchlen));
								matchlen = 0;
							}
							break;
						default:
							break;
					}
					switch ( *tc )
					{
						case STEP_MATCH:
							matchlen += 1;
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				if ( matchlen )
				{
					R.push_back(Match(apos-matchlen,bpos-matchlen,matchlen));
					matchlen = 0;
				}
			}

			static void getMatchOffsets(
				step_type const * ta,
				step_type const * te,
				libmaus2::autoarray::AutoArray < Match > & R,
				uint64_t & o,
				uint64_t const off_a = 0, uint64_t const off_b = 0
			)
			{
				uint64_t apos = off_a, bpos = off_b;
				uint64_t matchlen = 0;

				for ( step_type const * tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MISMATCH:
						case STEP_INS:
						case STEP_DEL:
							if ( matchlen )
							{
								R.push(o,Match(apos-matchlen,bpos-matchlen,matchlen));
								matchlen = 0;
							}
							break;
						default:
							break;
					}
					switch ( *tc )
					{
						case STEP_MATCH:
							matchlen += 1;
							apos += 1;
							bpos += 1;
							break;
						case STEP_MISMATCH:
							apos += 1;
							bpos += 1;
							break;
						case STEP_INS:
							bpos += 1;
							break;
						case STEP_DEL:
							apos += 1;
							break;
						case STEP_RESET:
							break;
					}
				}

				if ( matchlen )
				{
					R.push(o,Match(apos-matchlen,bpos-matchlen,matchlen));
					matchlen = 0;
				}
			}

			void
				getKMatchOffsets(unsigned int const k, std::vector < std::pair<uint64_t,uint64_t> > & R,  uint64_t const off_a = 0, uint64_t const off_b = 0) const
			{
				getKMatchOffsets(ta,te,k,R,off_a,off_b);
			}

			template<typename it>
			static std::string traceToString(it ta, it te)
			{
				std::ostringstream ostr;

				for ( it tc = ta; tc != te; ++tc )
				{
					switch ( *tc )
					{
						case STEP_MATCH:
							ostr.put('+');
							break;
						case STEP_MISMATCH:
							ostr.put('-');
							break;
						case STEP_INS:
							ostr.put('I');
							break;
						case STEP_DEL:
							ostr.put('D');
							break;
						case STEP_RESET:
							ostr.put('R');
							break;
						default:
							ostr.put('?');
							break;
					}
				}

				return ostr.str();
			}

			std::string traceToString() const
			{
				return traceToString(ta,te);
			}

			int32_t getTraceScore() const
			{
				return getTraceScore(ta,te);
			}

			bool operator==(AlignmentTraceContainer const & o) const
			{
				if ( getTraceLength() != o.getTraceLength() )
					return false;

				step_type *  tc =   ta;
				step_type * otc = o.ta;

				while ( tc != te )
					if ( *(tc++) != *(otc++) )
						return false;

				return true;
			}

			bool operator!=(AlignmentTraceContainer const & o) const
			{
				return ! operator==(o);
			}

			struct MapElement
			{
				uint64_t apos;
				int64_t adif;
				uint64_t bpos;
				int64_t bdif;
				step_type step;

				MapElement() {}
				MapElement(
					uint64_t const rapos,
					int64_t const radif,
					uint64_t const rbpos,
					int64_t const rbdif,
					step_type const rstep
				) : apos(rapos), adif(radif), bpos(rbpos), bdif(rbdif), step(rstep)
				{

				}

				std::ostream & print(std::ostream & out) const
				{
					return out << "MapElement("
						<< "apos=" << apos << ","
						<< "adif=" << adif << ","
						<< "bpos=" << bpos << ","
						<< "bdif=" << bdif << ","
						<< "step=" << step << ")";
				}

				std::string toString() const
				{
					std::ostringstream ostr;
					print(ostr);
					return ostr.str();
				}
			};

			static uint64_t fillMapping(
				step_type const * uta, step_type const * ute,
				uint64_t const apos,
				uint64_t const bpos,
				libmaus2::autoarray::AutoArray<MapElement> & A
			)
			{
				std::pair<uint64_t,uint64_t> const SL = getStringLengthUsed(uta,ute);

				uint64_t aipos = apos+SL.first;
				uint64_t bipos = bpos+SL.second;
				int64_t aidif = 0;
				int64_t bidif = 0;
				uint64_t f = 0;

				while ( ute != uta )
				{
					libmaus2::lcs::AlignmentTraceContainer::step_type const step = *(--ute);

					switch ( step )
					{
						case libmaus2::lcs::AlignmentTraceContainer::STEP_MATCH:
						case libmaus2::lcs::AlignmentTraceContainer::STEP_MISMATCH:
						{
							--aipos;
							--bipos;
							aidif = 0;
							bidif = 0;

							A.push(f,MapElement(aipos,aidif,bipos,bidif,step));

							break;
						}
						case libmaus2::lcs::AlignmentTraceContainer::STEP_INS:
						{
							--bipos;
							bidif = 0;
							--aidif;

							A.push(f,MapElement(aipos,aidif,bipos,bidif,step));

							break;
						}
						case libmaus2::lcs::AlignmentTraceContainer::STEP_DEL:
						{
							--aipos;
							aidif = 0;
							--bidif;

							A.push(f,MapElement(aipos,aidif,bipos,bidif,step));

							break;
						}
						default:
						{
							break;
						}
					}
				}

				assert ( aipos == apos );
				assert ( bipos == bpos );

				std::reverse(A.begin(),A.begin()+f);

				return f;
			}
		};

		struct AlignmentTraceContainerAllocator
		{
			AlignmentTraceContainerAllocator() {}

			AlignmentTraceContainer::shared_ptr_type operator()()
			{
				AlignmentTraceContainer::shared_ptr_type tptr(new AlignmentTraceContainer);
				return tptr;
			}
		};

		struct AlignmentTraceContainerTypeInfo
		{
			typedef AlignmentTraceContainerTypeInfo this_type;

			typedef libmaus2::lcs::AlignmentTraceContainer::shared_ptr_type pointer_type;

			static pointer_type getNullPointer()
			{
				pointer_type p;
				return p;
			}

			static pointer_type deallocate(pointer_type /* p */)
			{
				return getNullPointer();
			}
		};
	}
}
#endif
