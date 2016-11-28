/*
    libmaus2
    Copyright (C) 2009-2013 German Tischler
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
#if ! defined(LIBMAUS2_BAMBAM_DUPMARKBASE_HPP)
#define LIBMAUS2_BAMBAM_DUPMARKBASE_HPP

#include <libmaus2/bambam/BamHeaderUpdate.hpp>
#include <libmaus2/bambam/BamMergeCoordinate.hpp>
#include <libmaus2/bambam/OpticalComparator.hpp>
#include <libmaus2/bambam/ReadEnds.hpp>
#include <libmaus2/bambam/DupSetCallback.hpp>
#include <libmaus2/math/iabs.hpp>
#include <libmaus2/bambam/BamWriter.hpp>
#include <libmaus2/lz/BgzfRecode.hpp>
#include <libmaus2/lz/BgzfRecodeParallel.hpp>
#include <libmaus2/lz/BgzfDeflateOutputCallbackMD5.hpp>
#include <libmaus2/util/MemUsage.hpp>
#include <libmaus2/util/GetObject.hpp>
#include <libmaus2/bambam/BgzfDeflateOutputCallbackBamIndex.hpp>
#include <libmaus2/aio/InputStreamFactoryContainer.hpp>
#include <libmaus2/bambam/BamHeaderParserStateBase.hpp>
#include <libmaus2/bambam/BamAlignmentSnappyInput.hpp>

namespace libmaus2
{
	namespace bambam
	{
		struct DupMarkBase
		{
			// #define DEBUG

			#if defined(DEBUG)
			#define MARKDUPLICATEPAIRSDEBUG
			#define MARKDUPLICATEFRAGSDEBUG
			#define MARKDUPLICATECOPYALIGNMENTS
			#endif

			struct MarkDuplicateProjectorIdentity
			{
				static libmaus2::bambam::ReadEnds const & deref(libmaus2::bambam::ReadEnds const & object)
				{
					return object;
				}
			};

			struct MarkDuplicateProjectorPointerDereference
			{
				static libmaus2::bambam::ReadEnds const & deref(libmaus2::bambam::ReadEnds const * object)
				{
					return *object;
				}
			};

			struct MarkOptical
			{
				virtual ~MarkOptical() {}
				virtual void operator()(uint64_t const, uint64_t const) = 0;
			};

			template<typename iterator, typename projector, bool bmarkopt>
			static uint64_t markDuplicatePairsOpt(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				unsigned int const optminpixeldif,
				MarkOptical * markopt
			)
			{
				if ( lfrags_e-lfrags_a > 1 )
				{
					#if defined(MARKDUPLICATEPAIRSDEBUG)
					std::cerr << "[V] [D] --- checking for duplicate pairs ---" << std::endl;
					for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
						std::cerr << "[V] [D] considering " << projector::deref(*lfrags_c) << std::endl;
					#endif

					uint64_t maxscore = projector::deref(*lfrags_a).getScore();

					iterator lfrags_m = lfrags_a;
					for ( iterator lfrags_c = lfrags_a+1; lfrags_c != lfrags_e; ++lfrags_c )
						if ( projector::deref(*lfrags_c).getScore() > maxscore )
						{
							maxscore = projector::deref(*lfrags_c).getScore();
							lfrags_m = lfrags_c;
						}

					for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
						if ( lfrags_c != lfrags_m )
						{
							#if defined(MARKDUPLICATEPAIRSDEBUG)
							std::cerr << "[V] [D] marking " << projector::deref(*lfrags_c) << std::endl;
							#endif
							DSC(projector::deref(*lfrags_c));
						}

					// check for optical duplicates
					std::sort ( lfrags_a, lfrags_e, ::libmaus2::bambam::OpticalComparator() );

					for ( iterator low = lfrags_a; low != lfrags_e; )
					{
						iterator high = low+1;

						// search top end of tile
						while (
							high != lfrags_e &&
							// same read group
							projector::deref(*high).getReadGroup() == projector::deref(*low).getReadGroup() &&
							// same tile
							projector::deref(*high).getTile() == projector::deref(*low).getTile() )
						{
							++high;
						}

						if (
							// more than one
							high-low > 1
							&&
							// tile not undefined
							projector::deref(*low).getTile()
						)
						{
							#if defined(DEBUG)
							std::cerr << "[D] Range " << high-low << std::endl; // << " for " << projector::deref(*low) << std::endl;
							#endif

							std::vector<bool> opt(high-low,false);
							std::vector<uint64_t> optref(high-low);
							bool haveoptdup = false;

							for ( iterator i = low; i+1 != high; ++i )
							{
								for (
									iterator j = i+1;
									j != high
									&&
									// within range for X?
									projector::deref(*j).getX() - projector::deref(*i).getX() <= optminpixeldif;
									++j
								)
									// within range for Y?
									if (
										::libmaus2::math::iabs(
											static_cast<int64_t>(projector::deref(*i).getY())
											-
											static_cast<int64_t>(projector::deref(*j).getY())
										)
										<= optminpixeldif
									)
									{
										// mark j as optical duplicate in Boolean vector
										opt [ j - low ] = true;
										optref [ j - low ] = i - low;
										// we have found at least one optical duplicate
										haveoptdup = true;
									}
							}

							if ( haveoptdup )
							{
								unsigned int const lib = projector::deref(*low).getLibraryId();
								uint64_t numopt = 0;
								for ( uint64_t i = 0; i < opt.size(); ++i )
									if ( opt[i] )
									{
										numopt++;

										if ( bmarkopt )
										{
											ReadEndsBase const & REB = projector::deref(low[i]);
											ReadEndsBase const & REBref = projector::deref(low[optref[i]]);
											uint64_t const i1 = REB.getRead1IndexInFile();
											uint64_t const i2 = REB.getRead2IndexInFile();
											uint64_t const iref1 = REBref.getRead1IndexInFile();
											uint64_t const iref2 = REBref.getRead2IndexInFile();
											(*markopt)(i1,iref1);
											(*markopt)(i2,iref2);
										}
									}

								DSC.addOpticalDuplicates(lib,numopt);
							}
						}

						low = high;
					}
				}
				else
				{
					#if defined(MARKDUPLICATEPAIRSDEBUG)
					std::cerr << "[V] --- singleton pair set ---" << std::endl;
					for ( iterator i = lfrags_a; i != lfrags_e; ++i )
						std::cerr << "[V] " << projector::deref(*i) << std::endl;
					#endif

				}

				uint64_t const lfragssize = lfrags_e-lfrags_a;
				// all but one are duplicates
				return lfragssize ? 2*(lfragssize - 1) : 0;
			}

			template<typename iterator, typename projector>
			static uint64_t markDuplicatePairs(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairsOpt<iterator,projector,false /* bmarkopt */>(lfrags_a,lfrags_e,DSC,optminpixeldif,0 /* mark opt */);
			}

			template<typename iterator, typename projector>
			static uint64_t markDuplicatePairs(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				MarkOptical * markopt,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairsOpt<iterator,projector,true /* bmarkopt */>(lfrags_a,lfrags_e,DSC,optminpixeldif,markopt);
			}

			template<typename iterator>
			static uint64_t markDuplicatePairs(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<iterator,MarkDuplicateProjectorIdentity>(lfrags_a,lfrags_e,DSC,optminpixeldif);
			}

			template<typename iterator>
			static uint64_t markDuplicatePairs(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				MarkOptical * markopt,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<iterator,MarkDuplicateProjectorIdentity>(lfrags_a,lfrags_e,DSC,markopt,optminpixeldif);
			}

			static uint64_t markDuplicatePairsVector(
				std::vector< ::libmaus2::bambam::ReadEnds > & lfrags,
				::libmaus2::bambam::DupSetCallback & DSC,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<std::vector<libmaus2::bambam::ReadEnds>::iterator,MarkDuplicateProjectorIdentity>(lfrags.begin(),lfrags.end(),DSC,optminpixeldif);
			}

			static uint64_t markDuplicatePairsVector(
				std::vector< ::libmaus2::bambam::ReadEnds > & lfrags,
				::libmaus2::bambam::DupSetCallback & DSC,
				MarkOptical * markopt,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<std::vector<libmaus2::bambam::ReadEnds>::iterator,MarkDuplicateProjectorIdentity>(lfrags.begin(),lfrags.end(),DSC,markopt,optminpixeldif);
			}

			template<typename iterator>
			static uint64_t markDuplicatePairsPointers(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<iterator,MarkDuplicateProjectorPointerDereference>(lfrags_a,lfrags_e,DSC,optminpixeldif);
			}

			template<typename iterator>
			static uint64_t markDuplicatePairsPointers(
				iterator const lfrags_a,
				iterator const lfrags_e,
				::libmaus2::bambam::DupSetCallback & DSC,
				MarkOptical * markopt,
				unsigned int const optminpixeldif = 100
			)
			{
				return markDuplicatePairs<iterator,MarkDuplicateProjectorPointerDereference>(lfrags_a,lfrags_e,DSC,markopt,optminpixeldif);
			}

			static uint64_t markDuplicatePairs(
				std::vector< ::libmaus2::bambam::ReadEnds > & lfrags,
				::libmaus2::bambam::DupSetCallback & DSC
			)
			{
				return markDuplicatePairsVector(lfrags,DSC);
			}

			static uint64_t markDuplicatePairs(
				std::vector< ::libmaus2::bambam::ReadEnds > & lfrags,
				::libmaus2::bambam::DupSetCallback & DSC,
				MarkOptical * markopt
			)
			{
				return markDuplicatePairsVector(lfrags,DSC,markopt);
			}

			template<typename iterator, typename projector>
			static uint64_t markDuplicateFrags(iterator const lfrags_a, iterator const lfrags_e, ::libmaus2::bambam::DupSetCallback & DSC)
			{
				uint64_t const lfragssize = lfrags_e - lfrags_a;

				if ( lfragssize > 1 )
				{
					#if defined(MARKDUPLICATEFRAGSDEBUG)
					std::cerr << "[V] --- frag set --- " << std::endl;
					for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
						std::cerr << "[V] " << projector::deref(*lfrags_c) << std::endl;
					#endif

					bool containspairs = false;
					bool containsfrags = false;

					for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
						if ( projector::deref(*lfrags_c).isPaired() )
							containspairs = true;
						else
							containsfrags = true;

					// if there are any single fragments
					if ( containsfrags )
					{
						// mark single ends as duplicates if there are pairs
						if ( containspairs )
						{
							#if defined(MARKDUPLICATEFRAGSDEBUG)
							std::cerr << "[V] there are pairs, marking single ends as duplicates" << std::endl;
							#endif

							uint64_t dupcnt = 0;
							// std::cerr << "Contains pairs." << std::endl;
							for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
								if ( ! (projector::deref(*lfrags_c).isPaired()) )
								{
									DSC(projector::deref(*lfrags_c));
									dupcnt++;
								}

							return dupcnt;
						}
						// if all are single keep highest score only
						else
						{
							#if defined(MARKDUPLICATEFRAGSDEBUG)
							std::cerr << "[V] there are only fragments, keeping best one" << std::endl;
							#endif
							// std::cerr << "Frags only." << std::endl;

							uint64_t maxscore = projector::deref(*lfrags_a).getScore();
							iterator lfrags_m = lfrags_a;

							for ( iterator lfrags_c = lfrags_a+1; lfrags_c != lfrags_e; ++lfrags_c )
								if ( projector::deref(*lfrags_c).getScore() > maxscore )
								{
									maxscore = projector::deref(*lfrags_c).getScore();
									lfrags_m = lfrags_c;
								}

							for ( iterator lfrags_c = lfrags_a; lfrags_c != lfrags_e; ++lfrags_c )
								if ( lfrags_c != lfrags_m )
									DSC(projector::deref(*lfrags_c));

							return lfragssize-1;
						}
					}
					else
					{
						#if defined(MARKDUPLICATEFRAGSDEBUG)
						std::cerr << "[V] group does not contain unpaired reads." << std::endl;
						#endif

						return 0;
					}
				}
				else
				{
					return 0;
				}
			}

			template<typename iterator>
			static uint64_t markDuplicateFrags(iterator const lfrags_a, iterator const lfrags_e, ::libmaus2::bambam::DupSetCallback & DSC)
			{
				return markDuplicateFrags<iterator,MarkDuplicateProjectorIdentity>(lfrags_a,lfrags_e,DSC);
			}

			static uint64_t markDuplicateFrags(std::vector< ::libmaus2::bambam::ReadEnds > const & lfrags, ::libmaus2::bambam::DupSetCallback & DSC)
			{
				return markDuplicateFrags<std::vector< ::libmaus2::bambam::ReadEnds >::const_iterator,MarkDuplicateProjectorIdentity>(lfrags.begin(),lfrags.end(),DSC);
			}

			template<typename iterator>
			static uint64_t markDuplicateFragsPointers(iterator const lfrags_a, iterator const lfrags_e, ::libmaus2::bambam::DupSetCallback & DSC)
			{
				return markDuplicateFrags<iterator,MarkDuplicateProjectorPointerDereference>(lfrags_a,lfrags_e,DSC);
			}

			static bool isDupPair(::libmaus2::bambam::ReadEndsBase const & A, ::libmaus2::bambam::ReadEndsBase const & B)
			{
				bool const notdup =
					A.getLibraryId()       != B.getLibraryId()       ||
					A.getTagId()           != B.getTagId()           ||
					A.getRead1Sequence()   != B.getRead1Sequence()   ||
					A.getRead1Coordinate() != B.getRead1Coordinate() ||
					A.getOrientation()     != B.getOrientation()     ||
					A.getRead2Sequence()   != B.getRead2Sequence()   ||
					A.getRead2Coordinate() != B.getRead2Coordinate()
				;

				return ! notdup;
			}

			static bool isDupFrag(::libmaus2::bambam::ReadEndsBase const & A, ::libmaus2::bambam::ReadEndsBase const & B)
			{
				bool const notdup =
					A.getLibraryId()       != B.getLibraryId()       ||
					A.getTagId()           != B.getTagId()           ||
					A.getRead1Sequence()   != B.getRead1Sequence()   ||
					A.getRead1Coordinate() != B.getRead1Coordinate() ||
					A.getOrientation()     != B.getOrientation()
				;

				return ! notdup;
			}

			static void addBamDuplicateFlag(
				::libmaus2::util::ArgInfo const & arginfo,
				bool const verbose,
				::libmaus2::bambam::BamHeader const & bamheader,
				uint64_t const maxrank,
				uint64_t const mod,
				int const level,
				::libmaus2::bambam::DupSetCallback const & DSC,
				std::istream & in,
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > * Pcbs,
				std::string const & progid,
				std::string const & packageversion
			)
			{
				::libmaus2::bambam::BamHeader::unique_ptr_type uphead(::libmaus2::bambam::BamHeaderUpdate::updateHeader(arginfo,bamheader,progid,packageversion));

				::libmaus2::aio::OutputStream::unique_ptr_type pO;
				std::ostream * poutputstr = 0;

				if ( arginfo.hasArg("O") && (arginfo.getValue<std::string>("O","") != "") )
				{
					::libmaus2::aio::OutputStream::unique_ptr_type tpO(
						::libmaus2::aio::OutputStreamFactoryContainer::constructUnique(
							arginfo.getValue<std::string>("O",std::string("O"))
						)
					);
					pO = UNIQUE_PTR_MOVE(tpO);
					poutputstr = pO.get();
				}
				else
				{
					poutputstr = & std::cout;
				}

				std::ostream & outputstr = *poutputstr;

				/* write bam header */
				{
					libmaus2::lz::BgzfDeflate<std::ostream> headout(outputstr);
					for ( uint64_t i = 0; Pcbs && i < Pcbs->size(); ++i )
						headout.registerBlockOutputCallback(Pcbs->at(i));
					uphead->serialise(headout);
					headout.flush();
				}


				uint64_t const bmod = libmaus2::math::nextTwoPow(mod);
				uint64_t const bmask = bmod-1;

				libmaus2::timing::RealTimeClock globrtc; globrtc.start();
				libmaus2::timing::RealTimeClock locrtc; locrtc.start();
				libmaus2::lz::BgzfRecode rec(in,outputstr,level);
				for ( uint64_t i = 0; Pcbs && i < Pcbs->size(); ++i )
					rec.registerBlockOutputCallback(Pcbs->at(i));

				bool haveheader = false;
				bool recactive = false;
				uint64_t blockskip = 0;

				libmaus2::bambam::BamHeaderParserState bhps;

				std::cerr << "[D] using incremental BAM header parser on serial decoder." << std::endl;

				/* read  blocks until we have reached the end of the BAM header */
				while ( (!haveheader) && (recactive = !(rec.getBlockPlusEOF().second)) )
				{
					libmaus2::util::GetObject<uint8_t const *> G(rec.deflatebase.pa);
					std::pair<bool,uint64_t> const ps = bhps.parseHeader(G,rec.P.uncompdatasize);
					haveheader = ps.first;
					blockskip = ps.second; // bytes used for header in this block
				}

				if ( blockskip )
				{
					uint64_t const bytesused = (rec.deflatebase.pc - rec.deflatebase.pa) - blockskip;
					memmove ( rec.deflatebase.pa, rec.deflatebase.pa + blockskip, bytesused );
					rec.deflatebase.pc = rec.deflatebase.pa + bytesused;
					rec.P.uncompdatasize = bytesused;
					blockskip = 0;

					if ( ! bytesused )
						(recactive = !(rec.getBlockPlusEOF().second));
				}

				/* parser state types and variables */
				enum parsestate { state_reading_blocklen, state_pre_skip, state_marking, state_post_skip };
				parsestate state = state_reading_blocklen;
				unsigned int blocklenred = 0;
				uint32_t blocklen = 0;
				uint32_t preskip = 0;
				uint64_t alcnt = 0;
				unsigned int const dupflagskip = 15;
				// uint8_t const dupflagmask = static_cast<uint8_t>(~(4u));

				/* while we have alignment data blocks */
				while ( recactive )
				{
					uint8_t * pa       = rec.deflatebase.pa;
					uint8_t * const pc = rec.deflatebase.pc;

					while ( pa != pc )
						switch ( state )
						{
							/* read length of next alignment block */
							case state_reading_blocklen:
								/* if this is a little endian machine allowing unaligned access */
								#if defined(LIBMAUS2_HAVE_i386)
								if ( (!blocklenred) && ((pc-pa) >= static_cast<ptrdiff_t>(sizeof(uint32_t))) )
								{
									blocklen = *(reinterpret_cast<uint32_t const *>(pa));
									blocklenred = sizeof(uint32_t);
									pa += sizeof(uint32_t);

									state = state_pre_skip;
									preskip = dupflagskip;
								}
								else
								#endif
								{
									while ( pa != pc && blocklenred < sizeof(uint32_t) )
										blocklen |= static_cast<uint32_t>(*(pa++)) << ((blocklenred++)*8);

									if ( blocklenred == sizeof(uint32_t) )
									{
										state = state_pre_skip;
										preskip = dupflagskip;
									}
								}
								break;
							/* skip data before the part we modify */
							case state_pre_skip:
								{
									uint32_t const skip = std::min(pc-pa,static_cast<ptrdiff_t>(preskip));
									pa += skip;
									preskip -= skip;
									blocklen -= skip;

									if ( ! skip )
										state = state_marking;
								}
								break;
							/* change data */
							case state_marking:
								assert ( pa != pc );
								if ( DSC.isMarked(alcnt) )
								{
									*pa |=    4;
								}
								else
								{
									*pa &= 0xfb; // ~4
								}
								state = state_post_skip;
								// intended fall through to post_skip case
							/* skip data after part we modify */
							case state_post_skip:
							{
								uint32_t const skip = std::min(pc-pa,static_cast<ptrdiff_t>(blocklen));
								pa += skip;
								blocklen -= skip;

								if ( ! blocklen )
								{
									state = state_reading_blocklen;
									blocklenred = 0;
									blocklen = 0;
									alcnt++;

									if ( verbose && ((alcnt & (bmask)) == 0) )
									{
										std::cerr << "[V] Marked " << (alcnt+1) << " (" << (alcnt+1)/(1024*1024) << "," << static_cast<double>(alcnt+1)/maxrank << ")"
											<< " time " << locrtc.getElapsedSeconds()
											<< " total " << globrtc.formatTime(globrtc.getElapsedSeconds())
											<< " "
											<< libmaus2::util::MemUsage()
											<< std::endl;
										locrtc.start();
									}
								}
								break;
							}
						}

					rec.putBlock();
					(recactive = !(rec.getBlockPlusEOF().second));
				}

				rec.addEOFBlock();
				outputstr.flush();

				if ( verbose )
					std::cerr << "[V] Marked " << 1.0 << " total for marking time "
						<< globrtc.formatTime(globrtc.getElapsedSeconds())
						<< " "
						<< libmaus2::util::MemUsage()
						<< std::endl;
			}

			static void addBamDuplicateFlagParallel(
				::libmaus2::util::ArgInfo const & arginfo,
				bool const verbose,
				::libmaus2::bambam::BamHeader const & bamheader,
				uint64_t const maxrank,
				uint64_t const mod,
				int const level,
				::libmaus2::bambam::DupSetCallback const & DSC,
				std::istream & in,
				uint64_t const numthreads,
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > * Pcbs,
				std::string const & progid,
				std::string const & packageversion
			)
			{
				::libmaus2::bambam::BamHeader::unique_ptr_type uphead(::libmaus2::bambam::BamHeaderUpdate::updateHeader(arginfo,bamheader,progid,packageversion));

				::libmaus2::aio::OutputStream::unique_ptr_type pO;

				std::ostream * poutputstr = 0;

				if ( arginfo.hasArg("O") && (arginfo.getValue<std::string>("O","") != "") )
				{
					::libmaus2::aio::OutputStream::unique_ptr_type tpO(
						::libmaus2::aio::OutputStreamFactoryContainer::constructUnique(arginfo.getValue<std::string>("O",std::string("O")))
					);
					pO = UNIQUE_PTR_MOVE(tpO);
					poutputstr = pO.get();
				}
				else
				{
					poutputstr = & std::cout;
				}

				std::ostream & outputstr = *poutputstr;

				/* write bam header */
				{
					libmaus2::lz::BgzfDeflate<std::ostream> headout(outputstr);
					for ( uint64_t i = 0; Pcbs && i < Pcbs->size(); ++i )
						headout.registerBlockOutputCallback(Pcbs->at(i));
					uphead->serialise(headout);
					headout.flush();
				}

				libmaus2::lz::BgzfRecodeParallel rec(in,outputstr,level,numthreads,numthreads*4);
				for ( uint64_t i = 0; Pcbs && i < Pcbs->size(); ++i )
					rec.registerBlockOutputCallback(Pcbs->at(i));

				uint64_t const bmod = libmaus2::math::nextTwoPow(mod);
				uint64_t const bmask = bmod-1;

				libmaus2::timing::RealTimeClock globrtc; globrtc.start();
				libmaus2::timing::RealTimeClock locrtc; locrtc.start();

				bool haveheader = false;
				bool recactive = false;
				uint64_t blockskip = 0;

				libmaus2::bambam::BamHeaderParserState bhps;

				std::cerr << "[D] using incremental BAM header parser on parallel recoder." << std::endl;

				/* read  blocks until we have reached the end of the BAM header */
				while ( (!haveheader) && (recactive=rec.getBlock()) )
				{
					libmaus2::util::GetObject<uint8_t const *> G(rec.deflatebase.pa);
					std::pair<bool,uint64_t> const ps = bhps.parseHeader(G,rec.P.second);
					haveheader = ps.first;
					blockskip = ps.second; // bytes used for header in this block
				}

				if ( blockskip )
				{
					uint64_t const bytesused = (rec.deflatebase.pc - rec.deflatebase.pa) - blockskip;
					memmove ( rec.deflatebase.pa, rec.deflatebase.pa + blockskip, bytesused );
					rec.deflatebase.pc = rec.deflatebase.pa + bytesused;
					rec.P.second = bytesused;
					blockskip = 0;

					if ( ! bytesused )
						(recactive=rec.getBlock());
				}

				/* parser state types and variables */
				enum parsestate { state_reading_blocklen, state_pre_skip, state_marking, state_post_skip };
				parsestate state = state_reading_blocklen;
				unsigned int blocklenred = 0;
				uint32_t blocklen = 0;
				uint32_t preskip = 0;
				uint64_t alcnt = 0;
				unsigned int const dupflagskip = 15;
				// uint8_t const dupflagmask = static_cast<uint8_t>(~(4u));

				/* while we have alignment data blocks */
				while ( recactive )
				{
					uint8_t * pa       = rec.deflatebase.pa;
					uint8_t * const pc = rec.deflatebase.pc;

					while ( pa != pc )
						switch ( state )
						{
							/* read length of next alignment block */
							case state_reading_blocklen:
								/* if this is a little endian machine allowing unaligned access */
								#if defined(LIBMAUS2_HAVE_i386)
								if ( (!blocklenred) && ((pc-pa) >= static_cast<ptrdiff_t>(sizeof(uint32_t))) )
								{
									blocklen = *(reinterpret_cast<uint32_t const *>(pa));
									blocklenred = sizeof(uint32_t);
									pa += sizeof(uint32_t);

									state = state_pre_skip;
									preskip = dupflagskip;
								}
								else
								#endif
								{
									while ( pa != pc && blocklenred < sizeof(uint32_t) )
										blocklen |= static_cast<uint32_t>(*(pa++)) << ((blocklenred++)*8);

									if ( blocklenred == sizeof(uint32_t) )
									{
										state = state_pre_skip;
										preskip = dupflagskip;
									}
								}
								break;
							/* skip data before the part we modify */
							case state_pre_skip:
								{
									uint32_t const skip = std::min(pc-pa,static_cast<ptrdiff_t>(preskip));
									pa += skip;
									preskip -= skip;
									blocklen -= skip;

									if ( ! skip )
										state = state_marking;
								}
								break;
							/* change data */
							case state_marking:
								assert ( pa != pc );
								if ( DSC.isMarked(alcnt) )
									*pa |= 4;
								state = state_post_skip;
								// intended fall through to post_skip case
							/* skip data after part we modify */
							case state_post_skip:
							{
								uint32_t const skip = std::min(pc-pa,static_cast<ptrdiff_t>(blocklen));
								pa += skip;
								blocklen -= skip;

								if ( ! blocklen )
								{
									state = state_reading_blocklen;
									blocklenred = 0;
									blocklen = 0;
									alcnt++;

									if ( verbose && ((alcnt & (bmask)) == 0) )
									{
										std::cerr << "[V] Marked " << (alcnt+1) << " (" << (alcnt+1)/(1024*1024) << "," << static_cast<double>(alcnt+1)/maxrank << ")"
											<< " time " << locrtc.getElapsedSeconds()
											<< " total " << globrtc.formatTime(globrtc.getElapsedSeconds())
											<< " "
											<< libmaus2::util::MemUsage()
											<< std::endl;
										locrtc.start();
									}
								}
								break;
							}
						}

					rec.putBlock();
					(recactive=rec.getBlock());
				}

				rec.addEOFBlock();
				outputstr.flush();

				if ( verbose )
					std::cerr << "[V] Marked " << 1.0 << " total for marking time "
						<< globrtc.formatTime(globrtc.getElapsedSeconds())
						<< " "
						<< libmaus2::util::MemUsage()
						<< std::endl;
			}

			template<typename decoder_type>
			static void markDuplicatesInFileTemplate(
				::libmaus2::util::ArgInfo const & arginfo,
				bool const verbose,
				::libmaus2::bambam::BamHeader const & bamheader,
				uint64_t const maxrank,
				uint64_t const mod,
				int const level,
				::libmaus2::bambam::DupSetCallback const & DSC,
				decoder_type & decoder,
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > * Pcbs,
				std::string const & progid,
				std::string const & packageversion
			)
			{
				::libmaus2::bambam::BamHeader::unique_ptr_type uphead(::libmaus2::bambam::BamHeaderUpdate::updateHeader(arginfo,bamheader,progid,packageversion));

				::libmaus2::aio::OutputStream::unique_ptr_type pO;

				std::ostream * poutputstr = 0;

				if ( arginfo.hasArg("O") && (arginfo.getValue<std::string>("O","") != "") )
				{
					::libmaus2::aio::OutputStream::unique_ptr_type tpO(
						::libmaus2::aio::OutputStreamFactoryContainer::constructUnique(arginfo.getValue<std::string>("O",std::string("O")))
					);

					pO = UNIQUE_PTR_MOVE(tpO);
					poutputstr = pO.get();
				}
				else
				{
					poutputstr = & std::cout;
				}

				std::ostream & outputstr = *poutputstr;

				libmaus2::timing::RealTimeClock globrtc, locrtc;
				globrtc.start(); locrtc.start();
				uint64_t const bmod = libmaus2::math::nextTwoPow(mod);
				uint64_t const bmask = bmod-1;

				// rewrite file and mark duplicates
				::libmaus2::bambam::BamWriter::unique_ptr_type writer(new ::libmaus2::bambam::BamWriter(outputstr,*uphead,level,Pcbs));
				libmaus2::bambam::BamAlignment & alignment = decoder.getAlignment();
				uint32_t const dup = ::libmaus2::bambam::BamFlagBase::LIBMAUS2_BAMBAM_FDUP;
				uint32_t const notdup = ~dup;
				for ( uint64_t r = 0; decoder.readAlignment(); ++r )
				{
					if ( DSC.isMarked(r) )
						alignment.putFlags(alignment.getFlags() | dup);
					else
						alignment.putFlags(alignment.getFlags() & notdup);

					alignment.serialise(writer->getStream());

					if ( verbose && ((r+1) & bmask) == 0 )
					{
						std::cerr << "[V] Marked " << (r+1) << " (" << (r+1)/(1024*1024) << "," << static_cast<double>(r+1)/maxrank << ")"
							<< " time " << locrtc.getElapsedSeconds()
							<< " total " << globrtc.formatTime(globrtc.getElapsedSeconds())
							<< " "
							<< libmaus2::util::MemUsage()
							<< std::endl;
						locrtc.start();
					}
				}

				writer.reset();
				outputstr.flush();
				pO.reset();

				if ( verbose )
					std::cerr << "[V] Marked " << maxrank << "(" << maxrank/(1024*1024) << "," << 1 << ")" << " total for marking time " << globrtc.formatTime(globrtc.getElapsedSeconds())
						<< " "
						<< libmaus2::util::MemUsage()
						<< std::endl;
			}


			template<typename decoder_type, typename writer_type, typename dup_writer_type>
			static void removeDuplicatesFromFileTemplate(
				bool const verbose,
				uint64_t const maxrank,
				uint64_t const mod,
				::libmaus2::bambam::DupSetCallback const & DSC,
				decoder_type & decoder,
				writer_type & writer,
				dup_writer_type * dupwriter
			)
			{
				libmaus2::timing::RealTimeClock globrtc, locrtc;
				globrtc.start(); locrtc.start();
				uint64_t const bmod = libmaus2::math::nextTwoPow(mod);
				uint64_t const bmask = bmod-1;

				// rewrite file and mark duplicates
				libmaus2::bambam::BamAlignment & alignment = decoder.getAlignment();
				uint32_t const dup = ::libmaus2::bambam::BamFlagBase::LIBMAUS2_BAMBAM_FDUP;
				uint32_t const notdup = ~dup;
				for ( uint64_t r = 0; decoder.readAlignment(); ++r )
				{
					if ( ! DSC.isMarked(r) )
					{
						alignment.putFlags(alignment.getFlags() & notdup);
						alignment.serialise(writer.getStream());
					}
					else if ( dupwriter )
					{
						alignment.putFlags(alignment.getFlags() | dup);
						alignment.serialise(dupwriter->getStream());
					}

					if ( verbose && ((r+1) & bmask) == 0 )
					{
						std::cerr << "[V] Filtered " << (r+1) << " (" << (r+1)/(1024*1024) << "," << static_cast<double>(r+1)/maxrank << ")"
							<< " time " << locrtc.getElapsedSeconds()
							<< " total " << globrtc.formatTime(globrtc.getElapsedSeconds())
							<< " "
							<< libmaus2::util::MemUsage()
							<< std::endl;
						locrtc.start();
					}
				}

				if ( verbose )
					std::cerr << "[V] Filtered " << maxrank << "(" << maxrank/(1024*1024) << "," << 1 << ")" << " total for marking time " << globrtc.formatTime(globrtc.getElapsedSeconds())
						<< " "
						<< libmaus2::util::MemUsage()
						<< std::endl;

			}

			static void markDuplicatesInFile(
				::libmaus2::util::ArgInfo const & arginfo,
				bool const verbose,
				::libmaus2::bambam::BamHeader const & bamheader,
				uint64_t const maxrank,
				uint64_t const mod,
				int const level,
				::libmaus2::bambam::DupSetCallback const & DSC,
				std::string const & recompressedalignments,
				bool const rewritebam,
				std::string const & tmpfilenameindex,
				std::string const & progid,
				std::string const & packageversion,
				bool const defaultrmdup,
				bool const defaultmd5,
				bool const defaultindex,
				uint64_t const defaultmarkthreads
			)
			{
				bool const rmdup = arginfo.getValue<int>("rmdup",defaultrmdup);
				uint64_t const markthreads =
					std::max(
						static_cast<uint64_t>(1),arginfo.getValue<uint64_t>("markthreads",defaultmarkthreads)
					);

				bool const outputisfile = arginfo.hasArg("O") && (arginfo.getUnparsedValue("O","") != "");
				std::string outputfilename = outputisfile ? arginfo.getUnparsedValue("O","") : "";
				std::string dupoutputfilename = rmdup ? arginfo.getUnparsedValue("D","") : std::string();
				std::string md5filename;
				std::string indexfilename;
				std::string dupmd5filename;
				std::string dupindexfilename;

				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > cbs;
				::libmaus2::lz::BgzfDeflateOutputCallbackMD5::unique_ptr_type Pmd5cb;
				if ( arginfo.getValue<unsigned int>("md5",defaultmd5) )
				{
					if ( arginfo.hasArg("md5filename") &&  arginfo.getUnparsedValue("md5filename","") != "" )
						md5filename = arginfo.getUnparsedValue("md5filename","");
					else if ( outputisfile )
						md5filename = outputfilename + ".md5";
					else
						std::cerr << "[V] no filename for md5 given, not creating hash" << std::endl;

					if ( md5filename.size() )
					{
						::libmaus2::lz::BgzfDeflateOutputCallbackMD5::unique_ptr_type Tmd5cb(new ::libmaus2::lz::BgzfDeflateOutputCallbackMD5);
						Pmd5cb = UNIQUE_PTR_MOVE(Tmd5cb);
						cbs.push_back(Pmd5cb.get());
					}
				}
				libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex::unique_ptr_type Pindex;
				if ( arginfo.getValue<unsigned int>("index",defaultindex) )
				{
					if ( arginfo.hasArg("indexfilename") &&  arginfo.getUnparsedValue("indexfilename","") != "" )
						indexfilename = arginfo.getUnparsedValue("indexfilename","");
					else if ( outputisfile )
						indexfilename = outputfilename + ".bai";
					else
						std::cerr << "[V] no filename for index given, not creating index" << std::endl;

					if ( indexfilename.size() )
					{
						libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex::unique_ptr_type Tindex(new libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex(tmpfilenameindex));
						Pindex = UNIQUE_PTR_MOVE(Tindex);
						cbs.push_back(Pindex.get());
					}
				}
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > * Pcbs = 0;
				if ( cbs.size() )
					Pcbs = &cbs;

				/* dup file, if any */
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > dupcbs;
				::libmaus2::lz::BgzfDeflateOutputCallbackMD5::unique_ptr_type Pdupmd5cb;
				if ( dupoutputfilename.size() && arginfo.getValue<unsigned int>("dupmd5",defaultmd5) )
				{
					if ( arginfo.hasArg("dupmd5filename") &&  arginfo.getUnparsedValue("dupmd5filename","") != "" )
						dupmd5filename = arginfo.getUnparsedValue("dupmd5filename","");
					else
						dupmd5filename = dupoutputfilename + ".md5";

					if ( dupmd5filename.size() )
					{
						::libmaus2::lz::BgzfDeflateOutputCallbackMD5::unique_ptr_type Tdupmd5cb(new ::libmaus2::lz::BgzfDeflateOutputCallbackMD5);
						Pdupmd5cb = UNIQUE_PTR_MOVE(Tdupmd5cb);
						dupcbs.push_back(Pdupmd5cb.get());
					}
				}
				libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex::unique_ptr_type Pdupindex;
				if ( dupoutputfilename.size() && arginfo.getValue<unsigned int>("dupindex",defaultindex) )
				{
					if ( arginfo.hasArg("dupindexfilename") &&  arginfo.getUnparsedValue("dupindexfilename","") != "" )
						dupindexfilename = arginfo.getUnparsedValue("dupindexfilename","");
					else if ( outputisfile )
						dupindexfilename = dupoutputfilename + ".bai";

					if ( dupindexfilename.size() )
					{
						libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex::unique_ptr_type Tdupindex(new libmaus2::bambam::BgzfDeflateOutputCallbackBamIndex(tmpfilenameindex+"_dup"));
						Pdupindex = UNIQUE_PTR_MOVE(Tdupindex);
						dupcbs.push_back(Pdupindex.get());
					}
				}
				std::vector< ::libmaus2::lz::BgzfDeflateOutputCallback * > * Pdupcbs = 0;
				if ( dupcbs.size() )
					Pdupcbs = &dupcbs;

				#if 0
				cbs.push_back(&md5cb);
				cbs.push_back(&indexcb);
				#endif

				// remove duplicates
				if ( rmdup )
				{
					::libmaus2::bambam::BamHeader::unique_ptr_type uphead(::libmaus2::bambam::BamHeaderUpdate::updateHeader(arginfo,bamheader,progid,packageversion));

					bool const inputisbam = (arginfo.hasArg("I") && (arginfo.getValue<std::string>("I","") != "")) || rewritebam;

					::libmaus2::aio::OutputStream::unique_ptr_type pO;
					std::ostream * poutputstr = 0;

					if ( outputisfile )
					{
						::libmaus2::aio::OutputStream::unique_ptr_type tpO(
							::libmaus2::aio::OutputStreamFactoryContainer::constructUnique(outputfilename)
						);
						pO = UNIQUE_PTR_MOVE(tpO);
						poutputstr = pO.get();
					}
					else
					{
						poutputstr = & std::cout;
					}

					std::ostream & outputstr = *poutputstr;

					if ( inputisbam )
					{
						// multiple input files
						if ( arginfo.getPairCount("I") > 1 )
						{
							std::vector<std::string> const inputfilenames = arginfo.getPairValues("I");
							libmaus2::bambam::BamMergeCoordinate decoder(inputfilenames);
							decoder.disableValidation();
							::libmaus2::bambam::BamWriter::unique_ptr_type writer(new ::libmaus2::bambam::BamWriter(outputstr,*uphead,level,Pcbs));
							::libmaus2::bambam::BamWriter::unique_ptr_type dupwriter(dupoutputfilename.size() ? new ::libmaus2::bambam::BamWriter(dupoutputfilename,*uphead,level,Pdupcbs) : NULL);
							removeDuplicatesFromFileTemplate(verbose,maxrank,mod,DSC,decoder,*writer,dupwriter.get());
						}
						// single input file
						else
						{
							std::string const inputfilename =
								(arginfo.hasArg("I") && (arginfo.getValue<std::string>("I","") != ""))
								?
								arginfo.getValue<std::string>("I","I")
								:
								recompressedalignments;

							if ( markthreads < 2 )
							{
								::libmaus2::bambam::BamDecoder decoder(inputfilename);
								decoder.disableValidation();
								::libmaus2::bambam::BamWriter::unique_ptr_type writer(new ::libmaus2::bambam::BamWriter(outputstr,*uphead,level,Pcbs));
								::libmaus2::bambam::BamWriter::unique_ptr_type dupwriter(dupoutputfilename.size() ? new ::libmaus2::bambam::BamWriter(dupoutputfilename,*uphead,level,Pdupcbs) : NULL);
								removeDuplicatesFromFileTemplate(verbose,maxrank,mod,DSC,decoder,*writer,dupwriter.get());
							}
							else
							{
								libmaus2::aio::InputStream::unique_ptr_type pCIS(libmaus2::aio::InputStreamFactoryContainer::constructUnique(inputfilename));
								libmaus2::aio::InputStream & CIS = *pCIS;
								::libmaus2::bambam::BamHeaderUpdate UH(arginfo,progid,packageversion);
								libmaus2::bambam::BamParallelRewrite BPR(CIS,UH,outputstr,level,markthreads,libmaus2::bambam::BamParallelRewrite::getDefaultBlocksPerThread() /* blocks per thread */,Pcbs);
								libmaus2::bambam::BamAlignmentDecoder & dec = BPR.getDecoder();
								libmaus2::bambam::BamParallelRewrite::writer_type & writer = BPR.getWriter();
								::libmaus2::bambam::BamWriter::unique_ptr_type dupwriter(dupoutputfilename.size() ? new ::libmaus2::bambam::BamWriter(dupoutputfilename,*uphead,level,Pdupcbs) : NULL);
								removeDuplicatesFromFileTemplate(verbose,maxrank,mod,DSC,dec,writer,dupwriter.get());
							}
						}
					}
					else
					{
						libmaus2::bambam::BamAlignmentSnappyInput decoder(recompressedalignments);
						if ( verbose )
							std::cerr << "[V] Reading snappy alignments from " << recompressedalignments << std::endl;
						::libmaus2::bambam::BamWriter::unique_ptr_type writer(new ::libmaus2::bambam::BamWriter(outputstr,*uphead,level,Pcbs));
						::libmaus2::bambam::BamWriter::unique_ptr_type dupwriter(dupoutputfilename.size() ? new ::libmaus2::bambam::BamWriter(dupoutputfilename,*uphead,level,Pdupcbs) : NULL);
						removeDuplicatesFromFileTemplate(verbose,maxrank,mod,DSC,decoder,*writer,dupwriter.get());
					}

					outputstr.flush();
					pO.reset();
				}
				// mark duplicates
				else
				{
					// multiple input files
					if ( arginfo.getPairCount("I") > 1 )
					{
						std::vector<std::string> const inputfilenames = arginfo.getPairValues("I");
						libmaus2::bambam::BamMergeCoordinate decoder(inputfilenames);
						decoder.disableValidation();
						markDuplicatesInFileTemplate(arginfo,verbose,bamheader,maxrank,mod,level,DSC,decoder,Pcbs,progid,packageversion);
					}
					else if ( arginfo.hasArg("I") && (arginfo.getValue<std::string>("I","") != "") )
					{
						std::string const inputfilename = arginfo.getValue<std::string>("I","I");
						libmaus2::aio::InputStream::unique_ptr_type PPFIS(
							libmaus2::aio::InputStreamFactoryContainer::constructUnique(inputfilename)
						);
						libmaus2::aio::InputStream & PFIS = *PPFIS;

						if ( markthreads == 1 )
							addBamDuplicateFlag(arginfo,verbose,bamheader,maxrank,mod,level,DSC,PFIS /* CIS */,Pcbs,progid,packageversion);
						else
							addBamDuplicateFlagParallel(arginfo,verbose,bamheader,maxrank,mod,level,DSC,PFIS /* CIS */,markthreads,Pcbs,progid,packageversion);
					}
					else
					{
						if ( rewritebam )
						{
							libmaus2::aio::InputStream::unique_ptr_type pCIS(libmaus2::aio::InputStreamFactoryContainer::constructUnique(recompressedalignments));
							libmaus2::aio::InputStream & CIS = *pCIS;

							if ( markthreads == 1 )
								addBamDuplicateFlag(arginfo,verbose,bamheader,maxrank,mod,level,DSC,CIS,Pcbs,progid,packageversion);
							else
								addBamDuplicateFlagParallel(arginfo,verbose,bamheader,maxrank,mod,level,DSC,CIS,markthreads,Pcbs,progid,packageversion);
						}
						else
						{
							libmaus2::bambam::BamAlignmentSnappyInput decoder(recompressedalignments);
							if ( verbose )
								std::cerr << "[V] Reading snappy alignments from " << recompressedalignments << std::endl;
							markDuplicatesInFileTemplate(arginfo,verbose,bamheader,maxrank,mod,level,DSC,decoder,Pcbs,progid,packageversion);
						}
					}
				}

				if ( Pmd5cb )
				{
					Pmd5cb->saveDigestAsFile(md5filename);
				}
				if ( Pindex )
				{
					Pindex->flush(std::string(indexfilename));
				}
				if ( Pdupmd5cb )
				{
					Pdupmd5cb->saveDigestAsFile(dupmd5filename);
				}
				if ( Pdupindex )
				{
					Pdupindex->flush(std::string(dupindexfilename));
				}
			}
		};
	}
}
#endif
