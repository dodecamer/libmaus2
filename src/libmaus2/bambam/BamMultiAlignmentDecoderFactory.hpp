/*
    libmaus2
    Copyright (C) 2009-2015 German Tischler
    Copyright (C) 2011-2015 Genome Research Limited

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
#if ! defined(LIBMAUS2_BAMBAM_BAMMULTIALIGNMENTDECODERFACTORY_HPP)
#define LIBMAUS2_BAMBAM_BAMMULTIALIGNMENTDECODERFACTORY_HPP

#include <libmaus2/bambam/BamAlignmentDecoderFactory.hpp>
#include <libmaus2/bambam/BamMergeCoordinate.hpp>
#include <libmaus2/bambam/BamCat.hpp>

namespace libmaus2
{
	namespace bambam
	{
		struct BamMultiAlignmentDecoderFactory
		{
			typedef BamAlignmentDecoderFactory this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;

			std::vector<libmaus2::bambam::BamAlignmentDecoderInfo> const BADI;
			bool const putrank;

			BamMultiAlignmentDecoderFactory(std::vector<libmaus2::bambam::BamAlignmentDecoderInfo> const & rBADI, bool const rputrank = false) : BADI(rBADI), putrank(rputrank) {}
			virtual ~BamMultiAlignmentDecoderFactory() {}

			static std::set<std::string> getValidInputFormatsSet()
			{
				return libmaus2::bambam::BamAlignmentDecoderFactory::getValidInputFormatsSet();
			}

			static std::string getValidInputFormats()
			{
				return libmaus2::bambam::BamAlignmentDecoderFactory::getValidInputFormats();
			}

			libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type operator()() const
			{
				libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type tptr(construct(BADI,putrank));
				return UNIQUE_PTR_MOVE(tptr);
			}

			static libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type construct(
				std::vector<libmaus2::bambam::BamAlignmentDecoderInfo> const & BADI,
				bool const putrank = false,
				std::istream & istdin = std::cin,
				bool cat = false,
				bool streaming = false
			)
			{
				if ( ! BADI.size() || BADI.size() > 1 )
				{
					if ( cat )
					{
						libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type tptr(new libmaus2::bambam::BamCatWrapper(BADI,putrank,streaming));
						return UNIQUE_PTR_MOVE(tptr);
					}
					else
					{
						libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type tptr(new libmaus2::bambam::BamMergeCoordinate(BADI,putrank));
						return UNIQUE_PTR_MOVE(tptr);
					}
				}
				else
				{
					libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type tptr(
						libmaus2::bambam::BamAlignmentDecoderFactory::construct(BADI[0],putrank,istdin)
					);
					return UNIQUE_PTR_MOVE(tptr);
				}
			}

			static libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type construct(
				libmaus2::util::ArgInfo const & arginfo,
				bool const putrank = false,
				std::ostream * copystr = 0,
				std::istream & istdin = std::cin,
				bool cat = false,
				bool streaming = false
			)
			{
				std::vector<std::string> const I = arginfo.getPairValues("I");

				std::vector<libmaus2::bambam::BamAlignmentDecoderInfo> V;
				for ( uint64_t i = 0; i < I.size(); ++i )
					V.push_back(libmaus2::bambam::BamAlignmentDecoderInfo::constructInfo(arginfo,I[i],false /* put rank */,copystr));
				if ( ! I.size() )
					V.push_back(libmaus2::bambam::BamAlignmentDecoderInfo::constructInfo(arginfo,"-",false,copystr));

				libmaus2::bambam::BamAlignmentDecoderWrapper::unique_ptr_type tptr(construct(V,putrank,istdin,cat,streaming));
				return UNIQUE_PTR_MOVE(tptr);
			}
		};
	}
}
#endif
