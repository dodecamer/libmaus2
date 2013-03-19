/**
    libmaus
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
**/

#if ! defined(LIBMAUS_UTIL_NEGATIVEDIFFERENCEARRAY_HPP)
#define LIBMAUS_UTIL_NEGATIVEDIFFERENCEARRAY_HPP

#include <libmaus/util/Array832.hpp>
#include <libmaus/util/Array864.hpp>

namespace libmaus
{
	namespace util
	{
		struct NegativeDifferenceArray32
		{
			::libmaus::util::Array832::unique_ptr_type A;
			typedef NegativeDifferenceArray32 this_type;
			typedef ::libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			
			void serialise(std::ostream & out) const
			{
				A->serialise(out);
			}
			
			NegativeDifferenceArray32(::std::istream & in)
			: A ( new ::libmaus::util::Array832(in) )
			{
			
			}
			
			NegativeDifferenceArray32(::libmaus::util::Array832::unique_ptr_type & rA)
			: A(UNIQUE_PTR_MOVE(rA))
			{
			
			}
			
			uint32_t operator[](uint64_t const i) const
			{
				return i-(*A)[i];
			}
		};
		struct NegativeDifferenceArray64
		{
			::libmaus::util::Array864::unique_ptr_type A;
			typedef NegativeDifferenceArray64 this_type;
			typedef ::libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			
			void serialise(std::ostream & out) const
			{
				A->serialise(out);
			}
			
			NegativeDifferenceArray64(::std::istream & in)
			: A ( new ::libmaus::util::Array864(in) )
			{
			
			}
			
			NegativeDifferenceArray64(::libmaus::util::Array864::unique_ptr_type & rA)
			: A(UNIQUE_PTR_MOVE(rA))
			{
			
			}
			
			uint64_t operator[](uint64_t const i) const
			{
				return i-(*A)[i];
			}
		};
	}
}
#endif
