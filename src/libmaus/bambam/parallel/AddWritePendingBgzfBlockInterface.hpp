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
#if ! defined(LIBMAUS_BAMBAM_PARALLEL_ADDWRITEPENDINGBGZFBLOCKINTERFACE_HPP)
#define LIBMAUS_BAMBAM_PARALLEL_ADDWRITEPENDINGBGZFBLOCKINTERFACE_HPP

#include <libmaus/lz/BgzfDeflateOutputBufferBase.hpp>
#include <libmaus/lz/BgzfDeflateZStreamBaseFlushInfo.hpp>

namespace libmaus
{
	namespace bambam
	{
		namespace parallel
		{			
			struct AddWritePendingBgzfBlockInterface
			{
				virtual ~AddWritePendingBgzfBlockInterface() {}
				virtual void addWritePendingBgzfBlock(
					int64_t const blockid,
					int64_t const subid,
					libmaus::lz::BgzfDeflateOutputBufferBase::shared_ptr_type obuf,
					libmaus::lz::BgzfDeflateZStreamBaseFlushInfo const & info
				) = 0;
			};
		}
	}
}
#endif
