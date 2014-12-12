/*
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
*/
#if ! defined(LIBMAUS_LZ_BGZFDEFLATEOUTPUTBUFFERBASE_HPP)
#define LIBMAUS_LZ_BGZFDEFLATEOUTPUTBUFFERBASE_HPP

#include <libmaus/lz/BgzfDeflateHeaderFunctions.hpp>
#include <libmaus/autoarray/AutoArray.hpp>

namespace libmaus
{
	namespace lz
	{
		struct BgzfDeflateOutputBufferBase : public BgzfDeflateHeaderFunctions
		{
			typedef BgzfDeflateOutputBufferBase this_type;
			typedef libmaus::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus::util::shared_ptr<this_type>::type shared_ptr_type;
		
			::libmaus::autoarray::AutoArray<uint8_t> outbuf;

			BgzfDeflateOutputBufferBase(int const level) : outbuf(getOutBufSizeTwo(level),false) 
			{
				setupHeader(outbuf.begin());			
			} 
		};
	}
}
#endif
