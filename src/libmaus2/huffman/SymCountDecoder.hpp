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
#if ! defined(LIBMAUS2_HUFFMAN_SYMCOUNTDECODER_HPP)
#define LIBMAUS2_HUFFMAN_SYMCOUNTDECODER_HPP

#include <libmaus2/aio/InputStreamFactoryContainer.hpp>
#include <libmaus2/aio/OutputStreamFactoryContainer.hpp>
#include <libmaus2/bitio/BitIOInput.hpp>
#include <libmaus2/bitio/readElias.hpp>
#include <libmaus2/huffman/CanonicalEncoder.hpp>
#include <libmaus2/huffman/IndexDecoderDataArray.hpp>
#include <libmaus2/util/HistogramSet.hpp>
#include <libmaus2/util/TempFileRemovalContainer.hpp>
#include <libmaus2/huffman/IndexLoader.hpp>
#include <libmaus2/gamma/GammaEncoderBase.hpp>

#include <libmaus2/huffman/SymCount.hpp>
#include <libmaus2/huffman/SymCountRun.hpp>

namespace libmaus2
{
	namespace huffman
	{
		struct SymCountDecoder
		{
			typedef SymCountDecoder this_type;
			typedef ::libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef SymCount value_type;

			::libmaus2::huffman::IndexDecoderDataArray::unique_ptr_type Pidda;
			::libmaus2::huffman::IndexDecoderDataArray const & idda;

			::libmaus2::autoarray::AutoArray < SymCountRun > rlbuffer;

			libmaus2::autoarray::AutoArray<SymCount> symcntbuffer;
			SymCount * sa;
			SymCount * sc;
			SymCount * se;

			::libmaus2::util::unique_ptr<libmaus2::aio::InputStreamInstance>::type istr;

			#if defined(SLOWDEC)
			::libmaus2::bitio::StreamBitInputStream::unique_ptr_type SBIS;
			#else
			typedef ::libmaus2::huffman::BitInputBuffer4 sbis_type;
			sbis_type::unique_ptr_type SBIS;
			#endif

			uint64_t fileptr;
			uint64_t blockptr;

			bool openNewFile()
			{
				if ( fileptr < idda.data.size() ) // file ptr valid, does file exist?
				{
					assert ( blockptr < idda.data[fileptr].numentries ); // check block pointer

					// open new input file stream
					::libmaus2::util::unique_ptr<libmaus2::aio::InputStreamInstance>::type tistr(
                                                new libmaus2::aio::InputStreamInstance(idda.data[fileptr].filename));
					istr = UNIQUE_PTR_MOVE(tistr);

					// seek to position and check if we succeeded
					istr->clear();
					istr->seekg(idda.data[fileptr].getPos(blockptr),std::ios::beg);

					if ( static_cast<int64_t>(istr->tellg()) != static_cast<int64_t>(idda.data[fileptr].getPos(blockptr)) )
					{
						::libmaus2::exception::LibMausException se;
						se.getStream() << "SymCountDecoder::openNewFile(): Failed to seek in file "
							<< idda.data[fileptr].filename << std::endl;
						se.finish();
						throw se;
					}

					// set up bit input
					sbis_type::raw_input_ptr_type ript(new sbis_type::raw_input_type(*istr));
					sbis_type::unique_ptr_type tSBIS(
                                                        new sbis_type(ript,static_cast<uint64_t>(64*1024))
                                                );
					SBIS = UNIQUE_PTR_MOVE(tSBIS);

					return true;
				}
				else
				{
					return false;
				}
			}

			uint64_t getN() const
			{
				if ( idda.vvec.size() )
					return idda.vvec[idda.vvec.size()-1];
				else
					return 0;
			}

			// init by position in stream
			void init(uint64_t offset)
			{
				if ( offset < idda.vvec[idda.vvec.size()-1] )
				{
					::libmaus2::huffman::FileBlockOffset const FBO = idda.findVBlock(offset);
					fileptr = FBO.fileptr;
					blockptr = FBO.blockptr;
					uint64_t restoffset = FBO.offset;

					openNewFile();

					SymCount SC;

					// this would be quicker using run lengths
					while ( restoffset )
					{
						decode(SC);
						--restoffset;
					}
				}
			}

			SymCountDecoder(
				std::vector<std::string> const & rfilenames, uint64_t offset /* offset = 0 */, uint64_t const numthreads
			)
			:
			  Pidda(::libmaus2::huffman::IndexDecoderDataArray::construct(rfilenames,numthreads)),
			  idda(*Pidda),
			  sa(symcntbuffer.end()), sc(symcntbuffer.end()), se(symcntbuffer.end()),
			  fileptr(0), blockptr(0)
			{
				init(offset);
			}

			SymCountDecoder(
				::libmaus2::huffman::IndexDecoderDataArray const & ridda,
				uint64_t offset
			)
			:
			  Pidda(),
			  idda(ridda),
			  sa(symcntbuffer.end()), sc(symcntbuffer.end()), se(symcntbuffer.end()),
			  fileptr(0), blockptr(0)
			{
				init(offset);
			}

			SymCountDecoder(
				::libmaus2::huffman::IndexDecoderDataArray const & ridda,
				::libmaus2::huffman::IndexEntryContainerVector const * /* = 0 */,
				uint64_t offset /* = 0 */
			)
			:
			  Pidda(),
			  idda(ridda),
			  sa(symcntbuffer.end()), sc(symcntbuffer.end()), se(symcntbuffer.end()),
			  fileptr(0), blockptr(0)
			{
				init(offset);
			}

			void decodeSymSeq(uint64_t const numsymruns)
			{
				::libmaus2::autoarray::AutoArray< std::pair<int64_t, uint64_t> > symmap = ::libmaus2::huffman::CanonicalEncoder::deserialise(*SBIS);
				// construct decoder for symbols
				::libmaus2::huffman::CanonicalEncoder symdec(symmap);

				// decode symbols
				for ( uint64_t i = 0; i < numsymruns; ++i )
				{
					uint64_t const sym = symdec.fastDecode(*SBIS);
					rlbuffer[i].sym = sym;
				}
			}

			void decodeSymCntSeq(uint64_t const numsymruns)
			{
				// read huffman code maps
				bool const symcntescape = SBIS->readBit();
				::libmaus2::autoarray::AutoArray< std::pair<int64_t, uint64_t> > symcntmap = ::libmaus2::huffman::CanonicalEncoder::deserialise(*SBIS);

				// construct decoder for symbol runlengths
				::libmaus2::huffman::EscapeCanonicalEncoder::unique_ptr_type escsymcntdec;
				::libmaus2::huffman::CanonicalEncoder::unique_ptr_type symcntdec;
				if ( symcntescape )
				{
					::libmaus2::huffman::EscapeCanonicalEncoder::unique_ptr_type tescsymcntdec(new ::libmaus2::huffman::EscapeCanonicalEncoder(symcntmap));
					escsymcntdec = UNIQUE_PTR_MOVE(tescsymcntdec);
				}
				else
				{
					::libmaus2::huffman::CanonicalEncoder::unique_ptr_type tsymcntdec(new ::libmaus2::huffman::CanonicalEncoder(symcntmap));
					symcntdec = UNIQUE_PTR_MOVE(tsymcntdec);
				}

				// decode runlengths
				if ( symcntescape )
					for ( uint64_t i = 0; i < numsymruns; ++i )
					{
						uint64_t const cnt = escsymcntdec->fastDecode(*SBIS);
						rlbuffer[i].rlen = cnt;
					}
				else
					for ( uint64_t i = 0; i < numsymruns; ++i )
					{
						uint64_t const cnt = symcntdec->fastDecode(*SBIS);
						rlbuffer[i].rlen = cnt;
					}
			}

			void decodeCntCounts(uint64_t const numcntruns)
			{
				// read cnt values (gamma code)
				for ( uint64_t i = 0; i < numcntruns; ++i )
				{
					unsigned int nd = 0;
					while ( !SBIS->readBit() )
						++nd;

					uint64_t v = 1;
					for ( unsigned int j = 0; j < nd; ++j )
					{
						v <<= 1;
						v |= static_cast<uint64_t>(SBIS->readBit());
					}

					rlbuffer[i].cnt = (v-1);
				}
			}

			void decodeSBits(uint64_t const numsymruns)
			{
				for ( uint64_t i = 0; i < numsymruns; ++i )
					rlbuffer[i].sbit = SBIS->readBit();
			}

			uint64_t unpack(uint64_t const numsymruns)
			{
				uint64_t numsymcnt = 0;
				for ( uint64_t i = 0; i < numsymruns; ++i )
					numsymcnt += rlbuffer[i].rlen;

				symcntbuffer.ensureSize(numsymcnt);
				uint64_t o = 0;
				for ( uint64_t i = 0; i < numsymruns; ++i )
					for ( uint64_t j = 0; j < rlbuffer[i].rlen; ++j, ++o )
					{
						symcntbuffer[o].sym = rlbuffer[i].sym;
						symcntbuffer[o].cnt = rlbuffer[i].cnt;
						symcntbuffer[o].sbit = rlbuffer[i].sbit;
					}

				return numsymcnt;
			}

			bool fillBuffer()
			{
				bool newfile = false;

				// check if we need to open a new file
				while ( fileptr < idda.data.size() && blockptr == idda.data[fileptr].numentries )
				{
					fileptr++;
					blockptr = 0;
					newfile = true;
				}
				// we have reached the end, no more blocks
				if ( fileptr == idda.data.size() )
					return false;
				// open new file if necessary
				if ( newfile )
					openNewFile();

				// byte align stream
				SBIS->flush();

				// read block size
				uint64_t const numsymruns = ::libmaus2::bitio::readElias2(*SBIS);
				rlbuffer.ensureSize(numsymruns);

				decodeSymSeq(numsymruns);
				decodeSymCntSeq(numsymruns);
				decodeCntCounts(numsymruns);
				decodeSBits(numsymruns);

				// unpack run length encoding
				uint64_t const numsymcnt = unpack(numsymruns);

				SBIS->flush();

				sa = symcntbuffer.begin();
				sc = sa;
				se = symcntbuffer.begin() + numsymcnt;

				// next block
				blockptr++;

				return true;
			}

			inline bool decode(SymCount & SC)
			{
				if ( sc == se )
				{
					fillBuffer();
					if ( sc == se )
						return false;
				}

				SC = *(sc++);
				return true;
			}

			void putBack(SymCount const & SC)
			{
				assert ( sc != sa );
				*--sc = SC;
			}

			static uint64_t getLength(std::string const & filename)
			{
				libmaus2::huffman::IndexDecoderData IDD(filename);
				return IDD.vacc;
			};

			// get length of vector of files in symbols
			static uint64_t getLength(std::vector<std::string> const & filenames, uint64_t const numthreads)
			{
				libmaus2::parallel::PosixSpinLock lock;
				uint64_t s = 0;
				#if defined(_OPENMP)
				#pragma omp parallel for num_threads(numthreads)
				#endif
				for ( uint64_t i = 0; i < filenames.size(); ++i )
				{
					uint64_t const ls = getLength(filenames[i]);
					libmaus2::parallel::ScopePosixSpinLock slock(lock);
					s += ls;
				}
				return s;
			}
		};
	}
}
#endif
