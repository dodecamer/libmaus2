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
#if ! defined(LIBMAUS2_DIGEST_SHA2_256_SSE4_HPP)
#define LIBMAUS2_DIGEST_SHA2_256_SSE4_HPP

#include <libmaus2/digest/DigestBase.hpp>
#include <libmaus2/util/I386CacheLineSize.hpp>
#include <libmaus2/exception/LibMausException.hpp>
#include <libmaus2/autoarray/AutoArray.hpp>

namespace libmaus2
{
	namespace digest
	{
		struct SHA2_256_sse4 : public DigestBase<32,6 /* block size 64 shift */, true /* need padding */, 8 /* number length */, true>
		{
			typedef DigestBase<32,6 /* block size 64 shift */, true /* need padding */, 8 /* number length */, true> base_type;
			typedef SHA2_256_sse4 this_type;

			// temp block
			libmaus2::autoarray::AutoArray<uint8_t,libmaus2::autoarray::alloc_type_memalign_cacheline> block;
			// digest (state)
			libmaus2::autoarray::AutoArray<uint32_t,libmaus2::autoarray::alloc_type_memalign_cacheline> digestw;
			// init data
			libmaus2::autoarray::AutoArray<uint32_t,libmaus2::autoarray::alloc_type_memalign_cacheline> digestinit;
			// index in current block
			uint64_t index;
			// number of completed blocks
			uint64_t blockcnt;

			SHA2_256_sse4();
			~SHA2_256_sse4();

			void init();
			void update(uint8_t const * t, size_t l);
			void digest(uint8_t * digest);
			void copyFrom(SHA2_256_sse4 const & O);
			static size_t getDigestLength() { return digestlength; }

			void vinit();
			void vupdate(uint8_t const * u, size_t l);
		};
	}
}
#endif
