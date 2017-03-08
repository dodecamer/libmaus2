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

#if ! defined(FASTWRITEBITWRITER_HPP)
#define FASTWRITEBITWRITER_HPP

#include <libmaus2/types/types.hpp>
#include <libmaus2/bitio/OutputBuffer.hpp>
#include <libmaus2/util/unique_ptr.hpp>
#include <libmaus2/math/lowbits.hpp>
#include <libmaus2/math/numbits.hpp>
#include <libmaus2/aio/SynchronousGenericOutputPosix.hpp>
#include <libmaus2/aio/SynchronousGenericOutput.hpp>
#include <iterator>
#include <vector>

namespace libmaus2
{
	namespace bitio
	{
		template<typename N, unsigned int b, bool full>
		struct DownShiftTemplate
		{
		};
		template<typename N, unsigned int b>
		struct DownShiftTemplate<N,b,true>
		{
			static N downShift(N const /* v */)
			{
				return N();
			}
		};
		template<typename N, unsigned int b>
		struct DownShiftTemplate<N,b,false>
		{
			static N downShift(N const v)
			{
				return v >> b;
			}
		};

		/**
		 * bit stream writer class
		 **/
		template<typename _data_type, typename _data_iterator, _data_type basemask, _data_type fullmask, bool _write_safe>
		struct FastWriteBitWriterTemplate
		{
			public:
			/**
			 * data type used by this bit writer class
			 **/
			typedef _data_type data_type;
			typedef _data_iterator data_iterator;

			typedef FastWriteBitWriterTemplate<data_type,data_iterator,basemask,fullmask,_write_safe> this_type;
			typedef typename ::libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;

			private:
			data_iterator U;
			data_type mask;
			data_type cur;
			unsigned int bitsleft;

			public:
			/**
			 * initialize writer with pointer to array
			 **/
			FastWriteBitWriterTemplate(data_iterator rU) : U(rU), mask(basemask), cur(0), bitsleft(8 * sizeof(data_type)) {}

			void reset(data_iterator rU)
			{
				U = rU;
				mask = basemask;
				cur = 0;
				bitsleft = 8 * sizeof(data_type);
			}

			void writeUnary(uint64_t k)
			{
				for ( uint64_t i = 0; i < k; ++i )
					writeBit(0);
				writeBit(1);
			}

			/**
			 *
			 **/
			template<typename N>
			void writeElias2(N n)
			{
				// number of bits to store n
				unsigned int log_1 = ::libmaus2::math::numbits(n);
				// number of bits to store log_1
				unsigned int log_2 = ::libmaus2::math::numbits(log_1);

				// std::cerr << "Writing " << n << " log_1=" << log_1 << " log_2=" << log_2 << std::endl;

				// write log_2 in unary form
				writeUnary(log_2);

				// write log_1 using log_2 bits
				write(log_1,log_2);

				// write n using log_1 bits
				write(n,log_1);
			}

			void reset()
			{
				mask = basemask;
				cur = 0;
				bitsleft = 8*sizeof(data_type);
			}

			void writeCurrent()
			{
				*(U++) = cur;
				reset();
			}

			static std::string curToBitString(uint64_t const cur)
			{
				std::ostringstream ostr;
				for ( uint64_t i = 0; i < 8*sizeof(data_type); ++i )
				{
					bool const bit = cur & (1ull << (8*sizeof(data_type)-i-1));
					ostr << bit;
				}
				return ostr.str();
			}

			std::string curToBitString()
			{
				return curToBitString(cur);
			}

			/**
			 * write a b bit number n
			 * @param n number to be written
			 * @param b number of bits to write
			 **/
			template<typename N>
			void writeInternal(N n, unsigned int b)
			{
				if ( b < bitsleft )
				{
					cur |= static_cast<data_type>(n) << (bitsleft - b);
					bitsleft -= b;
					mask >>= b;
				}
				else
				{
					cur |= static_cast<data_type>(n >> (b-bitsleft));
					b -= bitsleft;
					writeCurrent();
					write<N>(n & ::libmaus2::math::lowbits(b) , b);
				}
			}

			/**
			 * write a b bit number n
			 * @param n number to be written
			 * @param b number of bits to write
			 **/
			template<typename N>
			void write(N n, unsigned int b)
			{
				if ( _write_safe && (b&(~31u)) )
				{
					assert ( b >= 32 );
					unsigned int const bottombits = 32;
					unsigned int const topbits    = b-bottombits;

					writeInternal(DownShiftTemplate<N,bottombits,bottombits>=CHAR_BIT*sizeof(N)>::downShift(n), topbits   );
					writeInternal(n & 0xFFFFFFFFUL, bottombits);
				}
				else
				{
					writeInternal(n,b);
				}
			}

			/**
			 * write one bit to stream
			 * @param bit
			 **/
			void writeBit(bool const bit)
			{
				if ( bit )
				{
					cur |= mask;
				}

				mask >>= 1;
				bitsleft -= 1;

				if ( ! mask )
					writeCurrent();
			}
			/**
			 * flush output (align to byte boundary) by writing zero bits
			 **/
			void flush()
			{
				while ( mask != basemask )
					writeBit(0);
			}
		};

		typedef FastWriteBitWriterTemplate<uint8_t,   uint8_t *,              0x80    ,       0xFF           , false> FastWriteBitWriter;
		typedef FastWriteBitWriterTemplate<uint16_t, uint16_t *,            0x8000    ,     0xFFFF           , false> FastWriteBitWriter2;
		typedef FastWriteBitWriterTemplate<uint32_t, uint32_t *,        0x80000000    , 0xFFFFFFFFul         , false> FastWriteBitWriter4;
		typedef FastWriteBitWriterTemplate<uint64_t, uint64_t *, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true> FastWriteBitWriter8;
		typedef FastWriteBitWriterTemplate<uint8_t , std::back_insert_iterator< std::vector<uint8_t> >, 0x80, 0xFF, false> FastWriteBitWriterVector8;
		typedef FastWriteBitWriterTemplate<uint64_t , std::back_insert_iterator< std::vector<uint64_t> >, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true> FastWriteBitWriterVector64;
		typedef FastWriteBitWriterTemplate<uint8_t , std::ostream_iterator < uint8_t >, 0x80, 0xFF, false > FastWriteBitWriterStream8;
		typedef FastWriteBitWriterTemplate<uint64_t , OutputBufferIterator<uint64_t>, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true> FastWriteBitWriterBuffer64;
		typedef FastWriteBitWriterTemplate<uint8_t , ::libmaus2::aio::SynchronousGenericOutputPosix<uint8_t>::iterator_type, 0x80, 0xFF, false > FastWriteBitWriterStream8Posix;
		typedef FastWriteBitWriterTemplate<uint8_t , ::libmaus2::aio::SynchronousGenericOutput<uint8_t>::iterator_type, 0x80, 0xFF,false > FastWriteBitWriterStream8Std;
		typedef FastWriteBitWriterTemplate<uint64_t , ::libmaus2::aio::SynchronousGenericOutputPosix<uint64_t>::iterator_type, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true> FastWriteBitWriterBuffer64PosixSync;
		typedef FastWriteBitWriterTemplate<uint64_t , ::libmaus2::aio::SynchronousGenericOutput<uint64_t>::iterator_type, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true> FastWriteBitWriterBuffer64Sync;
		typedef FastWriteBitWriterTemplate<uint32_t , ::libmaus2::aio::SynchronousGenericOutputPosix<uint32_t>::iterator_type, 0x80000000ULL, 0xFFFFFFFFULL, false> FastWriteBitWriterBuffer32PosixSync;
		typedef FastWriteBitWriterTemplate<uint32_t , ::libmaus2::aio::SynchronousGenericOutput<uint32_t>::iterator_type, 0x80000000ULL, 0xFFFFFFFFULL, false> FastWriteBitWriterBuffer32Sync;
		typedef FastWriteBitWriterTemplate<uint64_t , ::libmaus2::aio::SynchronousGenericOutput<uint64_t>::iterator_type, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, true > FastWriteBitWriterStream64Std;
	}
}
#endif
