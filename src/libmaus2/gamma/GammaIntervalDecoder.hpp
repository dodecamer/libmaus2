/*
    libmaus2
    Copyright (C) 2016 German Tischler

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
#if ! defined(LIBMAUS2_GAMMA_GAMMAINTERVALDECODER_HPP)
#define LIBMAUS2_GAMMA_GAMMAINTERVALDECODER_HPP

#include <libmaus2/huffman/IndexDecoderDataArray.hpp>
#include <libmaus2/gamma/GammaDecoder.hpp>
#include <libmaus2/aio/SynchronousGenericInput.hpp>

namespace libmaus2
{
	namespace gamma
	{
		struct GammaIntervalDecoder
		{
			typedef GammaIntervalDecoder this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;

			std::vector<std::string> const Vfn;
			libmaus2::huffman::IndexDecoderDataArray::unique_ptr_type Pindex;
			libmaus2::huffman::IndexDecoderDataArray & index;

			uint64_t fileptr;
			uint64_t blockptr;

			libmaus2::aio::InputStreamInstance::unique_ptr_type istr;

			typedef uint64_t gamma_data_type;
			typedef libmaus2::aio::SynchronousGenericInput<gamma_data_type> stream_type;
			typedef stream_type::unique_ptr_type stream_ptr_type;
			stream_ptr_type PSGI;

			typedef libmaus2::gamma::GammaDecoder<stream_type> gamma_decoder_type;
			typedef gamma_decoder_type::unique_ptr_type gamma_decoder_ptr_type;
			gamma_decoder_ptr_type PG;

			libmaus2::autoarray::AutoArray < std::pair<uint64_t,uint64_t> > Aintv;
			std::pair<uint64_t,uint64_t> * pa;
			std::pair<uint64_t,uint64_t> * pc;
			std::pair<uint64_t,uint64_t> * pe;

			void openNewFile()
			{
				if ( fileptr < index.data.size() ) // file ptr valid, does file exist?
				{
					assert ( blockptr < index.data[fileptr].numentries ); // check block pointer
					// open new input file stream

					libmaus2::aio::InputStreamInstance::unique_ptr_type tistr(
						new libmaus2::aio::InputStreamInstance(index.data[fileptr].filename));
					istr = UNIQUE_PTR_MOVE(tistr);

					// seek to position and check if we succeeded
					uint64_t const pos = index.data[fileptr].getPos(blockptr);
					uint64_t const bitsperword = 8*sizeof(uint64_t);

					uint64_t const fullwordsoffset = pos / bitsperword;
					uint64_t const bytesoffset = fullwordsoffset * sizeof(uint64_t);
					uint64_t const bitoffset = pos - bitsperword * fullwordsoffset;
					assert ( bitoffset < 64 );

					istr->clear();
					istr->seekg(bytesoffset,std::ios::beg);

					stream_ptr_type TSGI(new stream_type(*istr,8*1024,std::numeric_limits<uint64_t>::max(),false /* check mod */));
					PSGI = UNIQUE_PTR_MOVE(TSGI);

		                        gamma_decoder_ptr_type TG(new gamma_decoder_type(*PSGI));
		                        PG = UNIQUE_PTR_MOVE(TG);

					if ( bitoffset )
						PG->decodeWord(bitoffset);
				}
			}

			bool fillBuffer()
			{
				bool newfile = false;

				// check if we need to open a new file
				while ( fileptr < index.data.size() && blockptr == index.data[fileptr].numentries )
				{
					fileptr++;
					blockptr = 0;
					newfile = true;
				}

				// we have reached the end, no more blocks
				if ( fileptr == index.data.size() )
					return false;
				// open new file if necessary
				if ( newfile )
					openNewFile();

				uint64_t const numintv = PG->decode()+1;
				Aintv.ensureSize(numintv);

				pa = Aintv.begin();
				pc = Aintv.begin();
				pe = Aintv.begin() + numintv;

				uint64_t firstlow = PG->decode();
				uint64_t firstwidth = PG->decode()+1;
				pa[0].first = firstlow;
				pa[0].second = firstlow + firstwidth;

				for ( uint64_t i = 1; i < numintv; ++i )
				{
					pa[i].first = pa[i-1].first + PG->decode()+1;
					pa[i].second = pa[i].first + PG->decode()+1;
				}

				blockptr += 1;

				return true;
			}

			bool getNext(std::pair<uint64_t,uint64_t> & P)
			{
				if ( pc != pe )
				{
					P = *(pc++);
					return true;
				}
				else
				{
					fillBuffer();

					if ( pc == pe )
						return false;

					P = *(pc++);
					return true;
				}
			}

			GammaIntervalDecoder(std::vector<std::string> const & rVfn, uint64_t const voffset, uint64_t const numthreads)
			: Vfn(rVfn), Pindex(new libmaus2::huffman::IndexDecoderDataArray(Vfn,numthreads)), index(*Pindex), fileptr(0),
			  Aintv(), pa(Aintv.begin()), pc(Aintv.begin()), pe(Aintv.begin())
			{
				libmaus2::huffman::FileBlockOffset FBO = index.findVBlock(voffset);
				fileptr = FBO.fileptr;
				blockptr = FBO.blockptr;
				uint64_t restoffset = FBO.offset;

				openNewFile();

				bool const ok = fillBuffer();

				if ( ok )
				{
					while ( pc != pe && restoffset >= (pc->second-pc->first) )
					{
						restoffset -= pc->second-pc->first;
						++pc;
					}

					assert ( pc != pe );
					assert ( restoffset < (pc->second-pc->first) );
				}
				else
				{
					if ( fileptr < index.data.size() )
					{
						libmaus2::exception::LibMausException lme;
						lme.getStream()
							<< "[E] GammaIntervalDecoder(Vfn=";
						for ( uint64_t i = 0; i < Vfn.size(); ++i )
							lme.getStream() << "[" << Vfn[i] << "]";
						lme.getStream() << ",voffset=" << voffset << ",numthreads=" << numthreads;
						lme.getStream() << "): fileptr=" << fileptr << " != " << Vfn.size() << std::endl;
						lme.getStream() << "blockptr=" << blockptr << " restoffset=" << restoffset << std::endl;
						lme.getStream() << "index.data.size()=" << index.data.size() << std::endl;
						lme.finish();
						throw lme;
					}
				}
			}
		};
	}
}
#endif
