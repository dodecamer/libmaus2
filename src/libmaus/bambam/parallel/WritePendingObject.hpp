/*
    libmaus
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
#if ! defined(LIBMAUS_BAMBAM_PARALLEL_WRITEPENDINGOBJECT_HPP)
#define LIBMAUS_BAMBAM_PARALLEL_WRITEPENDINGOBJECT_HPP

#include <libmaus/lz/BgzfDeflateOutputBufferBase.hpp>
#include <libmaus/lz/BgzfDeflateZStreamBaseFlushInfo.hpp>

namespace libmaus
{
	namespace bambam
	{
		namespace parallel
		{
			struct WritePendingObject
			{
				std::ostream * out;
				int64_t blockid;
				int64_t subid;
				libmaus::lz::BgzfDeflateOutputBufferBase::shared_ptr_type obuf;
				libmaus::lz::BgzfDeflateZStreamBaseFlushInfo flushinfo;
				
				WritePendingObject() {}
				WritePendingObject(
					std::ostream * rout,
					int64_t const rblockid,
					int64_t const rsubid,
					libmaus::lz::BgzfDeflateOutputBufferBase::shared_ptr_type & robuf,
					libmaus::lz::BgzfDeflateZStreamBaseFlushInfo const & rflushinfo
				) : out(rout), blockid(rblockid), subid(rsubid), obuf(robuf), flushinfo(rflushinfo)
				{
				
				}
			};
		}
	}
}
#endif
