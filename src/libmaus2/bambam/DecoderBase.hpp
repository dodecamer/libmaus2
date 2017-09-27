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
#if ! defined(LIBMAUS2_BAMBAM_DECODERBASE_HPP)
#define LIBMAUS2_BAMBAM_DECODERBASE_HPP

#include <libmaus2/types/types.hpp>
#include <libmaus2/exception/LibMausException.hpp>

namespace libmaus2
{
	namespace bambam
	{
		/**
		 * decoder base class
		 **/
		struct DecoderBase
		{
			/**
			 * get next byte from stream; throws exception on EOF
			 *
			 * @param in input stream
			 * @return next byte
			 **/
			template<typename stream_type>
			static uint8_t getByte(stream_type & in)
			{
				int const c = in.get();

				if ( c < 0 )
				{
					::libmaus2::exception::LibMausException se;
					se.getStream() << "Unexpected EOF in ::libmaus2::bambam::DecoderBase::getByte()" << std::endl;
					se.finish();
					throw se;
				}

				return c;
			}

			/**
			 * get next byte from stream as a word; throws exception on EOF
			 *
			 * @param in input stream
			 * @return next byte as word
			 **/
			template<typename stream_type>
			static uint64_t getByteAsWord(stream_type & in)
			{
				return getByte(in);
			}

			/**
			 * get l byte little endian integer from in
			 *
			 * @param in input stream
			 * @param l length of number
			 * @return decoded number
			 **/
			template<typename stream_type>
			static uint64_t getLEInteger(stream_type & in, unsigned int const l)
			{
				uint64_t v = 0;
				for ( unsigned int i = 0; i < l; ++i )
					v |= getByteAsWord(in) << (8*i);
				return v;
			}

			/**
			 * get l byte little endian integer from D
			 *
			 * @param D input array
			 * @param l length of number
			 * @return decoded number
			 **/
			static uint64_t getLEInteger(uint8_t const * D, unsigned int const l)
			{
				#if defined(LIBMAUS2_HAVE_i386)
				switch ( l )
				{
					case 1: return *D;
					case 2: return *(reinterpret_cast<uint16_t const *>(D));
					case 4: return *(reinterpret_cast<uint32_t const *>(D));
					case 8: return *(reinterpret_cast<uint64_t const *>(D));
					default:
					{
						uint64_t v = 0;
						for ( unsigned int i = 0; i < l; ++i )
							v |= static_cast<uint64_t>(D[i]) << (8*i);
						return v;
					}
				}
				#else
				uint64_t v = 0;
				for ( unsigned int i = 0; i < l; ++i )
					v |= static_cast<uint64_t>(D[i]) << (8*i);
				return v;
				#endif
			}

			/**
			 * get l byte little endian integer from D
			 *
			 * @param D input array
			 * @param l length of number
			 * @return decoded number
			 **/
			static uint64_t getLEInteger(uint8_t * D, unsigned int const l)
			{
				return getLEInteger(D,l);
			}
		};
	}
}
#endif
