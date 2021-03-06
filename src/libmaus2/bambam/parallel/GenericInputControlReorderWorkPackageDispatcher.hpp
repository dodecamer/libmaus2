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
#if ! defined(LIBMAUS2_BAMBAM_PARALLEL_GENERICINPUTCONTROLREORDERWORKPACKAGEDISPATCHER_HPP)
#define LIBMAUS2_BAMBAM_PARALLEL_GENERICINPUTCONTROLREORDERWORKPACKAGEDISPATCHER_HPP

#include <libmaus2/bambam/parallel/RefIdInterval.hpp>
#include <libmaus2/bambam/parallel/GenericInputControlReorderWorkPackageFinishedInterface.hpp>
#include <libmaus2/bambam/parallel/GenericInputControlReorderWorkPackageReturnInterface.hpp>
#include <libmaus2/bambam/parallel/GenericInputControlReorderWorkPackage.hpp>
#include <libmaus2/parallel/SimpleThreadWorkPackageDispatcher.hpp>
#include <libmaus2/bambam/parallel/ChecksumsInterfaceGetInterface.hpp>
#include <libmaus2/bambam/parallel/ChecksumsInterfacePutInterface.hpp>

namespace libmaus2
{
	namespace bambam
	{
		namespace parallel
		{
			struct GenericInputControlReorderWorkPackageDispatcher : public libmaus2::parallel::SimpleThreadWorkPackageDispatcher
			{
				typedef GenericInputControlReorderWorkPackage this_type;
				typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
				typedef libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;

				GenericInputControlReorderWorkPackageReturnInterface & packageReturnInterface;
				GenericInputControlReorderWorkPackageFinishedInterface & finishedInterface;
				ChecksumsInterfaceGetInterface & checksumGetInterface;
				ChecksumsInterfacePutInterface & checksumPutInterface;
				libmaus2::bambam::BamAuxFilterVector filter;
				libmaus2::bitio::BitVector * BV;
				bool const gcomputerefidintervals;

				GenericInputControlReorderWorkPackageDispatcher(
					GenericInputControlReorderWorkPackageReturnInterface & rpackageReturnInterface,
					GenericInputControlReorderWorkPackageFinishedInterface & rfinishedInterface,
					ChecksumsInterfaceGetInterface & rchecksumGetInterface,
					ChecksumsInterfacePutInterface & rchecksumPutInterface,
					libmaus2::bitio::BitVector * rBV,
					bool rcomputerefidintervals
				)
				: packageReturnInterface(rpackageReturnInterface), finishedInterface(rfinishedInterface),
				  checksumGetInterface(rchecksumGetInterface),
				  checksumPutInterface(rchecksumPutInterface),
				  BV(rBV),
				  gcomputerefidintervals(rcomputerefidintervals)
				{
					filter.set('Z','R');
				}

				template<bool havedupvec, bool computerefidintervals>
				void dispatchTemplate2(libmaus2::parallel::SimpleThreadWorkPackage * P, libmaus2::parallel::SimpleThreadPoolInterfaceEnqueTermInterface & /* tpi */)
				{
					assert ( dynamic_cast<GenericInputControlReorderWorkPackage *>(P) != 0 );
					GenericInputControlReorderWorkPackage * BP = dynamic_cast<GenericInputControlReorderWorkPackage *>(P);

					libmaus2::bambam::parallel::AlignmentBuffer::shared_ptr_type & in = BP->in;
					libmaus2::bambam::parallel::FragmentAlignmentBuffer::shared_ptr_type & out = BP->out;
					std::pair<uint64_t,uint64_t> const I = BP->I;
					uint64_t const index = BP->index;
					libmaus2::bambam::parallel::FragmentAlignmentBufferFragment & frag = *((*out)[index]);
					uint32_t const dupflag = libmaus2::bambam::BamFlagBase::LIBMAUS2_BAMBAM_FDUP;
					uint32_t const dupmask = ~dupflag;
					ChecksumsInterface::shared_ptr_type Schecksums = checksumGetInterface.getSeqChecksumsObject();

					int32_t prevrefid = std::numeric_limits<int32_t>::min();
					std::vector<RefIdInterval> refidintervals;

					for ( uint64_t i = I.first; i < I.second; ++i )
					{
						// get alignment block data
						std::pair<uint8_t const *,uint64_t> P = in->at(i);

						// get next offset in buffer
						uint64_t const o = frag.getOffset();

						if ( computerefidintervals )
						{
							int32_t const refid = libmaus2::bambam::BamAlignmentDecoderBase::getRefID(P.first);

							if ( refid != prevrefid )
							{
								refidintervals.push_back(RefIdInterval(refid,i-I.first,o));
								prevrefid = refid;
							}
						}

						// put alignment
						frag.pushAlignmentBlock(P.first,P.second);

						// output data pointer
						uint8_t * p = frag.getPointer(o);

						// mark as duplicate if in bit vector
						if ( havedupvec )
						{
							// get rank
							int64_t const rank = libmaus2::bambam::BamAlignmentDecoderBase::getRank(p+sizeof(uint32_t),P.second);

							libmaus2::bambam::BamAlignmentEncoderBase::putFlags(p+sizeof(uint32_t),libmaus2::bambam::BamAlignmentDecoderBase::getFlags(p+sizeof(uint32_t)) & dupmask);

							if ( rank >= 0 && BV->get(rank) )
								libmaus2::bambam::BamAlignmentEncoderBase::putFlags(
									p+sizeof(uint32_t),libmaus2::bambam::BamAlignmentDecoderBase::getFlags(p+sizeof(uint32_t)) | dupflag
								);
						}

						// filter out ZR tag
						uint32_t const fl = libmaus2::bambam::BamAlignmentDecoderBase::filterOutAux(p+sizeof(uint32_t),P.second,filter);
						// replace length
						frag.replaceLength(o,fl);

						assert ( P.second >= fl );

						frag.pullBack(P.second-fl);

						if ( Schecksums )
							Schecksums->update(frag.getPointer(o)+sizeof(uint32_t),fl);
					}

					if ( refidintervals.size() )
					{
						refidintervals.back().i_high = (I.second - I.first);
						refidintervals.back().b_high = frag.getOffset();

						for ( std::vector<RefIdInterval>::size_type i = 0; (i+1) < refidintervals.size(); ++i )
						{
							refidintervals[i].i_high = refidintervals[i+1].i_low;
							refidintervals[i].b_high = refidintervals[i+1].b_low;
						}

						frag.refidintervals = refidintervals;
					}

					if ( Schecksums )
						checksumPutInterface.returnSeqChecksumsObject(Schecksums);

					finishedInterface.genericInputControlReorderWorkPackageFinished(BP->in,BP->out);
					packageReturnInterface.genericInputControlReorderWorkPackageReturn(BP);
				}

				template<bool havedupvec>
				void dispatchTemplate1(libmaus2::parallel::SimpleThreadWorkPackage * P, libmaus2::parallel::SimpleThreadPoolInterfaceEnqueTermInterface & tpi)
				{
					if ( gcomputerefidintervals )
						dispatchTemplate2<havedupvec,true>(P,tpi);
					else
						dispatchTemplate2<havedupvec,false>(P,tpi);
				}

				void dispatch(libmaus2::parallel::SimpleThreadWorkPackage * P, libmaus2::parallel::SimpleThreadPoolInterfaceEnqueTermInterface & tpi)
				{
					if ( BV )
						dispatchTemplate1<true>(P,tpi);
					else
						dispatchTemplate1<false>(P,tpi);
				}
			};
		}
	}
}
#endif
