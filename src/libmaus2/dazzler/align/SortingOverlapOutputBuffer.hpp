/*
    libmaus2
    Copyright (C) 2015 German Tischler

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
#if ! defined(LIBMAUS2_DAZZLER_ALIGN_SORTINGOVERLAPOUTPUTBUFFER_HPP)
#define LIBMAUS2_DAZZLER_ALIGN_SORTINGOVERLAPOUTPUTBUFFER_HPP

#include <libmaus2/dazzler/align/SortingOverlapOutputBufferMerger.hpp>
#include <libmaus2/dazzler/align/SortingOverlapBlockInput.hpp>
#include <libmaus2/dazzler/align/OverlapHeapComparator.hpp>
#include <libmaus2/dazzler/align/Overlap.hpp>
#include <libmaus2/dazzler/align/AlignmentFile.hpp>
#include <libmaus2/autoarray/AutoArray.hpp>
#include <libmaus2/aio/InputStreamInstance.hpp>
#include <libmaus2/util/TempFileRemovalContainer.hpp>
#include <libmaus2/aio/OutputStreamFactoryContainer.hpp>
#include <libmaus2/dazzler/align/AlignmentWriter.hpp>
#include <libmaus2/dazzler/align/OverlapMetaIteratorGet.hpp>
#include <queue>

namespace libmaus2
{
	namespace dazzler
	{
		namespace align
		{
			template<typename comparator_type = libmaus2::dazzler::align::OverlapComparator>
			struct SortingOverlapOutputBuffer
			{
				std::string const filename;
				bool const small;
				libmaus2::aio::OutputStreamInstance::unique_ptr_type Pout;
				libmaus2::autoarray::AutoArray<libmaus2::dazzler::align::Overlap> B;
				uint64_t f;
				std::streampos p;
				std::vector< std::pair<uint64_t,uint64_t> > blocks;
				comparator_type comparator;

				SortingOverlapOutputBuffer(std::string const & rfilename, bool const rsmall, uint64_t const rn, comparator_type rcomparator = comparator_type())
				: filename(rfilename), small(rsmall), Pout(new libmaus2::aio::OutputStreamInstance(filename)), B(rn), f(0), p(0), comparator(rcomparator)
				{

				}

				void flush()
				{
					std::streampos const pos = Pout->tellp();
					assert ( pos >= 0 );
					assert ( pos == p );

					if ( f )
					{
						std::stable_sort(B.begin(),B.begin()+f,comparator);

						for ( uint64_t i = 1; i < f; ++i )
						{
							bool const ok = !(comparator(B[i],B[i-1]));
							assert ( ok );
						}

						for ( uint64_t i = 0; i < f; ++i )
							p += B[i].serialiseWithPath(*Pout, small);

						blocks.push_back(std::pair<uint64_t,uint64_t>(pos,f));
						f = 0;
					}
				}

				void put(libmaus2::dazzler::align::Overlap const & OVL)
				{
					assert ( f < B.size() );
					B[f++] = OVL;
					if ( f == B.size() )
						flush();
				}

				typename SortingOverlapOutputBufferMerger<comparator_type>::unique_ptr_type getMerger()
				{
					flush();
					Pout.reset();
					typename SortingOverlapOutputBufferMerger<comparator_type>::unique_ptr_type tptr(new SortingOverlapOutputBufferMerger<comparator_type>(filename,small,blocks));
					return UNIQUE_PTR_MOVE(tptr);
				}

				void mergeToFile(std::ostream & out, std::iostream & indexstream, int64_t const tspace)
				{
					typename SortingOverlapOutputBufferMerger<comparator_type>::unique_ptr_type merger(getMerger());

					libmaus2::dazzler::align::AlignmentWriter writer(out,indexstream,tspace);
					libmaus2::dazzler::align::Overlap NOVL;
					uint64_t novl = 0;

					libmaus2::dazzler::align::Overlap OVLprev;
					bool haveprev = false;

					while ( merger->getNext(NOVL) )
					{
						if ( haveprev )
						{
							bool const ok = !(comparator(NOVL,OVLprev));
							assert ( ok );
						}

						writer.put(NOVL);
						novl += 1;

						haveprev = true;
						OVLprev = NOVL;
					}
				}

				void mergeToFile(std::string const & filename, int64_t const tspace)
				{
					std::string const indexfilename = libmaus2::dazzler::align::OverlapIndexer::getIndexName(filename);
					libmaus2::aio::OutputStreamInstance OSI(filename);
					libmaus2::aio::InputOutputStream::unique_ptr_type Pindexstream(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfilename,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
					mergeToFile(OSI,*Pindexstream,tspace);
				}

				static void sortFile(std::string const & infilename, std::string const & outfilename, uint64_t const n = 64*1024, std::string tmpfilename = std::string())
				{
					if ( ! tmpfilename.size() )
						tmpfilename = outfilename + ".S";

					libmaus2::util::TempFileRemovalContainer::addTempFile(tmpfilename);
					libmaus2::aio::InputStreamInstance::unique_ptr_type PISI(new libmaus2::aio::InputStreamInstance(infilename));
					libmaus2::dazzler::align::AlignmentFile algnfile(*PISI);
					libmaus2::dazzler::align::Overlap OVL;

					SortingOverlapOutputBuffer<comparator_type> SOOB(tmpfilename,algnfile.small,n);

					while ( algnfile.getNextOverlap(*PISI,OVL) )
						SOOB.put(OVL);

					SOOB.mergeToFile(outfilename,algnfile.tspace);

					libmaus2::aio::FileRemoval::removeFile(tmpfilename);
				}

				static std::vector<std::string> mergeFiles(
					std::vector<std::string> const & infilenames,
					std::string const & outfilenameprefix,
					uint64_t const numthreads,
					bool const regtmp = false,
					comparator_type comparator = comparator_type())
				{
					OverlapMetaIteratorGet G(infilenames);
					std::vector<uint64_t> const B = G.getBlockStarts(numthreads);
					int64_t const tspace = libmaus2::dazzler::align::AlignmentFile::getTSpace(infilenames);

					std::vector<std::string> VO(numthreads);
					libmaus2::parallel::PosixSpinLock L;

					#if defined(_OPENMP)
					#pragma omp parallel for schedule(dynamic,1) num_threads(numthreads)
					#endif
					for ( uint64_t i = 0; i < numthreads; ++i )
					{
						uint64_t const low = B[i];
						uint64_t const high = B[i+1];

						std::ostringstream fnostr;
						fnostr << outfilenameprefix << "." << std::setw(6) << std::setfill('0') << i << std::setw(0) << ".las";
						std::string const fn = fnostr.str();

						{
							libmaus2::parallel::ScopePosixSpinLock slock(L);
							VO[i] = fn;
							#if defined(SORTINGOVERLAPOUTPUTBUFFER_VERBOSE)
							std::cerr << "[D] writing [" << low << "," << high << ") to " << fn << std::endl;
							#endif
						}

						if ( regtmp )
						{
							libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
							libmaus2::util::TempFileRemovalContainer::addTempFile(libmaus2::dazzler::align::OverlapIndexer::getIndexName(fn));
						}

						libmaus2::dazzler::align::AlignmentWriter AW(fn,tspace);

						std::priority_queue<
							std::pair<uint64_t,libmaus2::dazzler::align::Overlap>,
							std::vector< std::pair<uint64_t,libmaus2::dazzler::align::Overlap> >,
							OverlapHeapComparator<comparator_type>
						> Q(comparator);

						libmaus2::autoarray::AutoArray<AlignmentFileRegion::unique_ptr_type> I(infilenames.size());
						Overlap OVL;
						for ( uint64_t i = 0; i < infilenames.size(); ++i )
						{
							AlignmentFileRegion::unique_ptr_type Tptr(libmaus2::dazzler::align::OverlapIndexer::openAlignmentFileRegion(infilenames[i],low,high));
							I[i] = UNIQUE_PTR_MOVE(Tptr);
							if ( I[i]->getNextOverlap(OVL) )
								Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(i,OVL));
						}

						bool haveprev = false;
						libmaus2::dazzler::align::Overlap OVLprev;

						while ( Q.size() )
						{
							std::pair<uint64_t,libmaus2::dazzler::align::Overlap> const P = Q.top();
							Q.pop();

							if ( haveprev )
							{
								bool const ok = !(comparator(P.second,OVLprev));
								assert ( ok );
							}

							AW.put(P.second);

							if ( I[P.first]->getNextOverlap(OVL) )
								Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(P.first,OVL));

							haveprev = true;
							OVLprev = P.second;
						}
					}

					return VO;
				}

				static void mergeFilesParallel(std::vector<std::string> const & infilenames, std::string const & outfilename, uint64_t const numthreads, comparator_type comparator = comparator_type())
				{
					// lock
					libmaus2::parallel::PosixSpinLock L;

					// get tspace and small
					int64_t const tspace = libmaus2::dazzler::align::AlignmentFile::getTSpace(infilenames);
					bool const small = libmaus2::dazzler::align::AlignmentFile::tspaceToSmall(tspace);

					// compute block starts for parallel merging
					OverlapMetaIteratorGet G(infilenames);
					std::vector<uint64_t> const B = G.getBlockStarts(numthreads);

					// total number of overlaps in output
					uint64_t gnumovl = 0;
					// block start byte positions in output
					libmaus2::autoarray::AutoArray<uint64_t> NB(numthreads+1);
					// block start alignment count in output
					libmaus2::autoarray::AutoArray<uint64_t> NA(numthreads+1);

					// compute NA and NB
					for ( uint64_t i = 0; i < numthreads; ++i )
					{
						// block high and low
						uint64_t const low = B[i];
						uint64_t const high = B[i+1];

						uint64_t numovl = 0;
						uint64_t numbytes = 0;

						for ( uint64_t j = 0; j < infilenames.size(); ++j )
						{
							libmaus2::dazzler::align::OverlapIndexer::OpenAlignmentFileRegionInfo info;
							AlignmentFileRegion::unique_ptr_type Tptr(libmaus2::dazzler::align::OverlapIndexer::openAlignmentFileRegion(infilenames[j],low,high,info));
							numovl += (info.algnhigh - info.algnlow);
							numbytes += (info.gposabove - info.gposbelow);

							#if 0
							uint64_t dnumovl = 0;
							std::streampos spbef = Tptr->Pfile->tellg();
							assert ( info.gposbelow == spbef );
							Overlap OVL;
							while ( Tptr->getNextOverlap(OVL) )
							{
								dnumovl += 1;
							}
							std::streampos spaft = Tptr->Pfile->tellg();
							bool const ok = ((spaft - spbef) == (info.gposabove - info.gposbelow));
							if ( ! ok )
							{
								std::cerr << "[E] expecting " << (info.gposabove - info.gposbelow) << " got real " << (spaft - spbef) << std::endl;
								assert ( ok );
							}

							bool const dok = (dnumovl == (info.algnhigh - info.algnlow));
							if ( ! dok )
							{
								std::cerr << "[E] algn expecting " << (info.algnhigh - info.algnlow) << " got " << dnumovl << std::endl;
								assert ( dok );
							}
							#endif
						}

						gnumovl += numovl;
						NB[i] = numbytes;
						NA[i] = numovl;
					}

					// compute prefix sums
					NB.prefixSums();
					for ( uint64_t i = 0; i < NB.size(); ++i )
						NB[i] += AlignmentFile::getSerialisedHeaderSize();
					NA.prefixSums();

					// open output file and write header
					libmaus2::aio::OutputStreamInstance::unique_ptr_type truncptr(new libmaus2::aio::OutputStreamInstance(outfilename));
					libmaus2::dazzler::align::AlignmentFile::serialiseHeader(*truncptr,gnumovl,tspace);
					// close output file
					truncptr.reset();

					// indexing base level mask
					uint64_t const base_level_mask = (1ull << OverlapIndexer::base_level_log)-1;

					// opening temp files for indexing
					std::vector<std::string> idxtempfilenames(numthreads);
					libmaus2::autoarray::AutoArray<libmaus2::aio::OutputStreamInstance::unique_ptr_type> Aindextmp(numthreads);
					for ( uint64_t i = 0; i < numthreads; ++i )
					{
						std::ostringstream fnostr;
						fnostr << outfilename << "_indextmp_" << i;
						std::string const fn = fnostr.str();
						libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
						idxtempfilenames[i] = fn;
						libmaus2::aio::OutputStreamInstance::unique_ptr_type Tptr(new libmaus2::aio::OutputStreamInstance(fn));
						Aindextmp[i] = UNIQUE_PTR_MOVE(Tptr);
					}

					// number of index entries per tmp output file
					std::vector<uint64_t> Vicnt(numthreads);
					
					int volatile failed = 0;
					libmaus2::parallel::PosixSpinLock failedlock;

					#if defined(_OPENMP)
					#pragma omp parallel for schedule(dynamic,1) num_threads(numthreads)
					#endif
					for ( uint64_t i = 0; i < numthreads; ++i )
					{
						try
						{
							// block low and high
							uint64_t const low = B[i];
							uint64_t const high = B[i+1];

							{
								libmaus2::parallel::ScopePosixSpinLock slock(L);
								#if defined(SORTINGOVERLAPOUTPUTBUFFER_VERBOSE)
								std::cerr << "[D] writing [" << low << "," << high << ")" << std::endl;
								#endif
							}

							// open output file
							libmaus2::aio::InputOutputStream::unique_ptr_type Optr(
								libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(outfilename,std::ios::in|std::ios::out|std::ios::binary)
							);
							// seek to start position for block
							Optr->seekp(NB[i]);
							// sanity check
							assert ( Optr->tellp() >= 0 );
							assert ( static_cast<int64_t>(Optr->tellp()) == static_cast<int64_t>(NB[i]) );
							// output pointer
							uint64_t pptr = NB[i];

							// get indexing tmp file
							libmaus2::aio::OutputStreamInstance & indextmp = *(Aindextmp[i]);

							// merge queue
							std::priority_queue<
								std::pair<uint64_t,libmaus2::dazzler::align::Overlap>,
								std::vector< std::pair<uint64_t,libmaus2::dazzler::align::Overlap> >,
								OverlapHeapComparator<comparator_type>
							> Q(comparator);

							// current alignment id/index in output file
							uint64_t algnid = NA[i];

							// open input files
							libmaus2::autoarray::AutoArray<AlignmentFileRegion::unique_ptr_type> I(infilenames.size());
							Overlap OVL;
							#if 0
							uint64_t ocnt = 0;
							#endif
							for ( uint64_t j = 0; j < infilenames.size(); ++j )
							{
								#if 0
								{
								AlignmentFileRegion::unique_ptr_type Tptr(libmaus2::dazzler::align::OverlapIndexer::openAlignmentFileRegion(infilenames[j],low,high));
								while ( Tptr->getNextOverlap(OVL) )
									++ocnt;
								}
								#endif

								AlignmentFileRegion::unique_ptr_type Tptr(libmaus2::dazzler::align::OverlapIndexer::openAlignmentFileRegion(infilenames[j],low,high));
								I[j] = UNIQUE_PTR_MOVE(Tptr);
								if ( I[j]->getNextOverlap(OVL) )
									Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(j,OVL));
							}
							#if 0
							if ( ocnt != NA[i+1]-NA[i]  )
							{
								std::cerr << "[E] ocnt=" << ocnt << " NA[i+1]-NA[i]=" << NA[i+1]-NA[i] << std::endl;
								assert ( ocnt == NA[i+1]-NA[i] );
							}
							#endif

							bool haveprev = false;
							libmaus2::dazzler::align::Overlap OVLprev;
							// number of index entries written
							uint64_t icnt = 0;

							uint64_t na = 0;
							while ( Q.size() )
							{
								std::pair<uint64_t,libmaus2::dazzler::align::Overlap> const P = Q.top();
								Q.pop();

								if ( haveprev )
								{
									bool const ok = !(comparator(P.second,OVLprev));
									assert ( ok );
								}

								if ( (((algnid) & base_level_mask) == 0) )
								{
									libmaus2::util::NumberSerialisation::serialiseNumber(indextmp,algnid);
									libmaus2::util::NumberSerialisation::serialiseNumber(indextmp,pptr);
									libmaus2::dazzler::align::OverlapMeta OVLmeta(P.second);
									OVLmeta.serialise(indextmp);
									icnt += 1;
								}
								algnid++;

								pptr += P.second.serialiseWithPath(*Optr,small);
								na += 1;

								if ( I[P.first]->getNextOverlap(OVL) )
									Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(P.first,OVL));

								haveprev = true;
								OVLprev = P.second;
							}

							assert ( na == (NA[i+1]-NA[i]) );

							// flush output file
							Optr->flush();

							// sanity checks, we should be at end of block now
							assert ( Optr->tellp() >= 0 );
							assert ( static_cast<int64_t>(Optr->tellp()) == static_cast<int64_t>(NB[i+1]));

							// close output file
							Optr.reset();

							// flush and close index file
							indextmp.flush();
							Aindextmp[i].reset();

							// store number of index entries written
							{
								libmaus2::parallel::ScopePosixSpinLock slock(L);
								Vicnt[i] = icnt;
							}
						}
						catch(std::exception const & ex)
						{
							libmaus2::parallel::ScopePosixSpinLock slock(libmaus2::aio::StreamLock::cerrlock);
							std::cerr << "[E] SortingOverlapOutputBuffer<>:mergeFilesParallel: " << ex.what() << std::endl;

							libmaus2::parallel::ScopePosixSpinLock fslock(failedlock);
							failed = 1;
						}
					}
					
					if ( failed )
					{
						libmaus2::exception::LibMausException lme;
						lme.getStream() << "[E] SortingOverlapOutputBuffer<>:mergeFilesParallel failed" << std::endl;
						lme.finish();
						throw lme;
					}

					// construct indexer
					typedef libmaus2::index::ExternalMemoryIndexGenerator<OverlapMeta,OverlapIndexerBase::base_level_log,OverlapIndexerBase::inner_level_log> indexer_type;
					indexer_type indexer(OverlapIndexer::getIndexName(outfilename));
					indexer.setup();

					// read back tmp index files and construct final index
					uint64_t runid = 0;
					for ( uint64_t i = 0; i < numthreads; ++i )
					{
						libmaus2::aio::InputStreamInstance istr(idxtempfilenames[i]);
						for ( uint64_t j = 0; j < Vicnt[i]; ++j )
						{
							uint64_t const algnid = libmaus2::util::NumberSerialisation::deserialiseNumber(istr);
							uint64_t const pptr = libmaus2::util::NumberSerialisation::deserialiseNumber(istr);
							libmaus2::dazzler::align::OverlapMeta OVLmeta(istr);
							indexer.put(OVLmeta, std::pair<uint64_t,uint64_t>(pptr,0));

							assert ( algnid == runid );
							runid += (1ull << OverlapIndexerBase::base_level_log);
						}
					}

					// remove tmp index files
					for ( uint64_t i = 0; i < numthreads; ++i )
						libmaus2::aio::FileRemoval::removeFile(idxtempfilenames[i]);

					// flush final index file
					indexer.flush();
				}

				static void removeFileAndIndex(std::string const infilename)
				{
					libmaus2::aio::FileRemoval::removeFile(infilename);
					libmaus2::aio::FileRemoval::removeFile(libmaus2::dazzler::align::OverlapIndexer::getIndexName(infilename));
				}

				static void removeFileAndIndex(std::vector<std::string> const & infilenames)
				{
					for ( uint64_t i = 0; i < infilenames.size(); ++i )
						removeFileAndIndex(infilenames[i]);
				}

				static void catFiles(std::vector<std::string> const & infilenames, std::string const & outfilename, bool const removeinput = false)
				{
					int64_t const tspace = libmaus2::dazzler::align::AlignmentFile::getTSpace(infilenames);
					libmaus2::dazzler::align::AlignmentWriter AW(outfilename,tspace);
					for ( uint64_t i = 0; i < infilenames.size(); ++i )
					{
						libmaus2::dazzler::align::AlignmentFileRegion::unique_ptr_type Pfile(libmaus2::dazzler::align::OverlapIndexer::openAlignmentFile(infilenames[i]));
						libmaus2::dazzler::align::Overlap OVL;
						while ( Pfile->getNextOverlap(OVL) )
							AW.put(OVL);
						Pfile.reset();
						if ( removeinput )
						{
							removeFileAndIndex(infilenames[i]);
						}
					}
				}

				static void mergeFiles(std::vector<std::string> const & infilenames, std::string const & outfilename, comparator_type comparator = comparator_type())
				{
					libmaus2::autoarray::AutoArray<libmaus2::aio::InputStreamInstance::unique_ptr_type> AISI(infilenames.size());
					libmaus2::autoarray::AutoArray<libmaus2::dazzler::align::AlignmentFile::unique_ptr_type> AF(infilenames.size());
					std::priority_queue<
						std::pair<uint64_t,libmaus2::dazzler::align::Overlap>,
						std::vector< std::pair<uint64_t,libmaus2::dazzler::align::Overlap> >,
						OverlapHeapComparator<comparator_type>
					> Q(comparator);
					libmaus2::dazzler::align::Overlap OVL;
					for ( uint64_t i = 0; i < AISI.size(); ++i )
					{
						libmaus2::aio::InputStreamInstance::unique_ptr_type tptr(new libmaus2::aio::InputStreamInstance(infilenames[i]));
						AISI[i] = UNIQUE_PTR_MOVE(tptr);
						libmaus2::dazzler::align::AlignmentFile::unique_ptr_type aptr(new libmaus2::dazzler::align::AlignmentFile(*(AISI[i])));
						AF[i] = UNIQUE_PTR_MOVE(aptr);

						if ( AF[i]->getNextOverlap(*(AISI[i]),OVL) )
							Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(i,OVL));
					}

					int64_t tspace = -1;
					uint64_t novl = 0;
					for ( uint64_t i = 0; i < AF.size(); ++i )
					{
						if ( tspace < 0 )
							tspace = AF[i]->tspace;
						else
							assert ( tspace == AF[i]->tspace );

						novl += AF[i]->novl;
					}

					libmaus2::dazzler::align::AlignmentWriter AW(outfilename,(tspace < 0) ? 0 : tspace,true /* create index */,novl);

					bool haveprev = false;
					libmaus2::dazzler::align::Overlap OVLprev;

					while ( Q.size() )
					{
						std::pair<uint64_t,libmaus2::dazzler::align::Overlap> const P = Q.top();
						Q.pop();

						if ( haveprev )
						{
							bool const ok = !(comparator(P.second,OVLprev));
							assert ( ok );
						}

						AW.put(P.second);

						if ( AF[P.first]->getNextOverlap(*(AISI[P.first]),OVL) )
							Q.push(std::pair<uint64_t,libmaus2::dazzler::align::Overlap>(P.first,OVL));

						haveprev = true;
						OVLprev = P.second;
					}
				}

				static uint64_t getDefaultMergeFanIn()
				{
					return 64u;
				}

				static void sortAndMerge(std::vector<std::string> infilenames, std::string const & outputfilename, std::string const & tmpfilebase, uint64_t const mergefanin = getDefaultMergeFanIn(), uint64_t const numthreads = 1, comparator_type comparator = comparator_type())
				{
					uint64_t volatile tmpid = 0;
					libmaus2::parallel::PosixSpinLock S;

					int volatile failed = 0;

					#if defined(_OPENMP)
					#pragma omp parallel for schedule(dynamic,1) num_threads(numthreads)
					#endif
					for ( uint64_t i = 0; i < infilenames.size(); ++i )
					{
						try
						{
							std::ostringstream fnostr;
							{
							libmaus2::parallel::ScopePosixSpinLock slock(S);
							fnostr << tmpfilebase << "_" << (tmpid++);
							}
							std::string const fn = fnostr.str();
							libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
							libmaus2::util::TempFileRemovalContainer::addTempFile(libmaus2::dazzler::align::OverlapIndexer::getIndexName(fn));
							libmaus2::dazzler::align::SortingOverlapOutputBuffer<comparator_type>::sortFile(infilenames[i],fn);

							libmaus2::parallel::ScopePosixSpinLock slock(S);
							infilenames[i] = fn;
						}
						catch(std::exception const & ex)
						{
							{
								libmaus2::parallel::ScopePosixSpinLock slock(libmaus2::aio::StreamLock::cerrlock);
								std::cerr << ex.what() << std::endl;
							}
							libmaus2::parallel::ScopePosixSpinLock slock(S);
							failed = 1;
						}
					}

					if ( failed )
					{
						libmaus2::exception::LibMausException lme;
						lme.getStream() << "SortingOverlapOutputBuffer::sortAndMerge: failed to sort single input files" << std::endl;
						lme.finish();
						throw lme;
					}

					while ( infilenames.size() > 1 )
					{
						std::vector<std::string> ninfilenames;

						uint64_t const numpack = (infilenames.size() + mergefanin - 1)/mergefanin;

						for ( uint64_t j = 0; j < numpack; ++j )
						{
							uint64_t const low = j * mergefanin;
							uint64_t const high = std::min(low+mergefanin,static_cast<uint64_t>(infilenames.size()));
							std::vector<std::string> tomerge(infilenames.begin()+low,infilenames.begin()+high);
							std::ostringstream fnostr;
							fnostr << tmpfilebase << "_" << (tmpid++);
							std::string const fn = fnostr.str();
							libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
							libmaus2::util::TempFileRemovalContainer::addTempFile(libmaus2::dazzler::align::OverlapIndexer::getIndexName(fn));
							if ( numthreads > 1 )
								libmaus2::dazzler::align::SortingOverlapOutputBuffer<comparator_type>::mergeFilesParallel(tomerge,fn,numthreads);
							else
								libmaus2::dazzler::align::SortingOverlapOutputBuffer<comparator_type>::mergeFiles(tomerge,fn,comparator);
							ninfilenames.push_back(fn);
							for ( uint64_t i = low; i < high; ++i )
							{
								libmaus2::aio::FileRemoval::removeFile(infilenames[i]);
								libmaus2::aio::FileRemoval::removeFile(libmaus2::dazzler::align::OverlapIndexer::getIndexName(infilenames[i]));
							}
						}

						infilenames = ninfilenames;
					}

					assert ( infilenames.size() == 1 );

					libmaus2::aio::OutputStreamFactoryContainer::rename(infilenames[0],outputfilename);
					libmaus2::aio::OutputStreamFactoryContainer::rename(
						libmaus2::dazzler::align::OverlapIndexer::getIndexName(infilenames[0]),
						libmaus2::dazzler::align::OverlapIndexer::getIndexName(outputfilename)
					);
				}
			};
		}
	}
}
#endif
