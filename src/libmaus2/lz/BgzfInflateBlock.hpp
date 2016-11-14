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
#if ! defined(LIBMAUS2_LZ_BGZFINFLATEBLOCK_HPP)
#define LIBMAUS2_LZ_BGZFINFLATEBLOCK_HPP

#include <libmaus2/lz/BgzfInflateBase.hpp>
#include <libmaus2/lz/BgzfInflateInfo.hpp>

namespace libmaus2
{
	namespace lz
	{
		struct BgzfInflateBlock : public BgzfInflateBase
		{
			typedef BgzfInflateBlock this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;

			enum bgzfinflateblockstate
			{
				bgzfinflateblockstate_idle,
				bgzfinflateblockstate_read_block,
				bgzfinflateblockstate_decompressed_block
			};

			::libmaus2::autoarray::AutoArray<uint8_t,::libmaus2::autoarray::alloc_type_memalign_pagesize> data;
			::libmaus2::lz::BgzfInflateInfo blockinfo;
			libmaus2::exception::LibMausException::unique_ptr_type ex;
			bgzfinflateblockstate state;

			uint64_t objectid;
			uint64_t blockid;

			BgzfInflateBlock(uint64_t const robjectid = 0)
			: data(getBgzfMaxBlockSize(),false), state(bgzfinflateblockstate_idle), objectid(robjectid), blockid(0)
			{

			}

			bool failed() const
			{
				return ex.get() != 0;
			}

			libmaus2::exception::LibMausException const & getException() const
			{
				assert ( failed() );
				return *ex;
			}

			/**
			 * read a block from stream
			 *
			 * @param stream input channel
			 * @return true in case of success, false for failure or EOF
			 **/
			template<typename stream_type>
			bool readBlock(stream_type & stream)
			{
				state = bgzfinflateblockstate_read_block;

				if ( failed() )
					return false;

				try
				{
					BgzfInflateBase::BaseBlockInfo preblockinfo = BgzfInflateBase::readBlock(stream);

					blockinfo = ::libmaus2::lz::BgzfInflateInfo(
						preblockinfo.compdatasize,
						preblockinfo.uncompdatasize,
						preblockinfo.uncompdatasize ? false : (stream.peek() == stream_type::traits_type::eof()),
						preblockinfo.checksum
					);

					return blockinfo.uncompressed;
				}
				catch(libmaus2::exception::LibMausException const & lex)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(lex.uclone());
					ex = UNIQUE_PTR_MOVE(tex);
					return false;
				}
				catch(std::exception const & lex)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(new libmaus2::exception::LibMausException);
					ex = UNIQUE_PTR_MOVE(tex);
					ex->getStream() << lex.what();
					ex->finish(false);
					return false;
				}
				catch(...)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(new libmaus2::exception::LibMausException);
					ex = UNIQUE_PTR_MOVE(tex);
					ex->getStream() << "BgzfInflateBlock::readBlock(): unknown exception caught";
					ex->finish(false);
					return false;
				}
			}

			/**
			 * decompress the currenctly buffered block
			 *
			 * @return number of uncompressed bytes in block, zero for EOF or failure
			 **/
			uint64_t decompressBlock()
			{
				state = bgzfinflateblockstate_decompressed_block;

				if ( failed() )
					return 0;

				try
				{
					BgzfInflateBase::decompressBlock(
						reinterpret_cast<char *>(data.begin()),
						BgzfInflateBase::BaseBlockInfo(
							blockinfo.compressed - libmaus2::lz::BgzfConstants::getBgzfHeaderSize() - libmaus2::lz::BgzfConstants::getBgzfFooterSize(),
							blockinfo.uncompressed,
							blockinfo.checksum,
							blockinfo.compressed
						)
					);
					return blockinfo.uncompressed;
				}
				catch(libmaus2::exception::LibMausException const & lex)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(lex.uclone());
					ex = UNIQUE_PTR_MOVE(tex);
					return 0;
				}
				catch(std::exception const & lex)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(new libmaus2::exception::LibMausException);
					ex = UNIQUE_PTR_MOVE(tex);
					ex->getStream() << lex.what();
					ex->finish(false);
					return 0;
				}
				catch(...)
				{
					libmaus2::exception::LibMausException::unique_ptr_type tex(new libmaus2::exception::LibMausException);
					ex = UNIQUE_PTR_MOVE(tex);
					ex->getStream() << "BgzfInflateBlock::decompressBlock(): unknown exception caught";
					ex->finish(false);
					return 0;
				}
			}

			/**
			 * read out the data in the decompressed block
			 *
			 * @param ldata buffer for storing the decompressed block
			 * @param n size of buffer ldata in bytes
			 * @return the number of uncompressed bytes in the buffer
			 **/
			uint64_t read(char * const ldata, uint64_t const n)
			{
				state = bgzfinflateblockstate_idle;

				if ( n < getBgzfMaxBlockSize() )
				{
					::libmaus2::exception::LibMausException se;
					se.getStream() << "BgzfInflate::decompressBlock(): provided buffer is too small: " << n << " < " << getBgzfMaxBlockSize();
					se.finish(false);
					throw se;
				}

				if ( failed() )
					throw getException();

				uint64_t const ndata = blockinfo.uncompressed;

				std::copy ( data.begin(), data.begin() + ndata, reinterpret_cast<uint8_t *>(ldata) );

				blockinfo = ::libmaus2::lz::BgzfInflateInfo(0,0,true,0 /* crc */);

				return ndata;
			}
		};
	}
}
#endif
