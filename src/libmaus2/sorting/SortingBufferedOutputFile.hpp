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
#if ! defined(LIBMAUS2_SORTING_SORTINGBUFFEREDOUTPUTFILE_HPP)
#define LIBMAUS2_SORTING_SORTINGBUFFEREDOUTPUTFILE_HPP

#include <libmaus2/sorting/MergingReadBack.hpp>
#include <libmaus2/aio/OutputStreamInstance.hpp>
#include <libmaus2/util/TempFileRemovalContainer.hpp>

namespace libmaus2
{
	namespace sorting
	{
		template<typename _data_type, typename _order_type = std::less<_data_type> >
		struct SortingBufferedOutputFile
		{
			typedef _data_type data_type;
			typedef _order_type order_type;
			typedef SortingBufferedOutputFile<data_type,order_type> this_type;
			typedef typename libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus2::sorting::MergingReadBack<data_type,order_type> merger_type;
			typedef typename libmaus2::sorting::MergingReadBack<data_type,order_type>::unique_ptr_type merger_ptr_type;

			std::string const filename;
			libmaus2::aio::OutputStreamInstance::unique_ptr_type PCOS;
			typename libmaus2::aio::SortingBufferedOutput<data_type,order_type>::unique_ptr_type SBO;

			SortingBufferedOutputFile(std::string const & rfilename, uint64_t const bufsize = 1024ull)
			: filename(rfilename), PCOS(new libmaus2::aio::OutputStreamInstance(filename)), SBO(new libmaus2::aio::SortingBufferedOutput<data_type,order_type>(*PCOS,bufsize))
			{
			}

			void put(data_type v)
			{
				SBO->put(v);
			}

			merger_ptr_type getMerger(uint64_t const backblocksize = 1024ull, uint64_t const maxfan = 16ull)
			{
				SBO->flush();
				std::vector<uint64_t> blocksizes = SBO->getBlockSizes();
				SBO.reset();
				PCOS->flush();
				PCOS.reset();

				blocksizes = libmaus2::sorting::MergingReadBack<data_type,order_type>::premerge(filename,blocksizes,maxfan,backblocksize);

				typename libmaus2::sorting::MergingReadBack<data_type,order_type>::unique_ptr_type ptr(
					new libmaus2::sorting::MergingReadBack<data_type,order_type>(filename,blocksizes,backblocksize)
				);

				return UNIQUE_PTR_MOVE(ptr);
			}
		};

		template<typename _data_type, typename _order_type = std::less<_data_type> >
		struct SerialisingSortingBufferedOutputFile
		{
			typedef _data_type data_type;
			typedef _order_type order_type;
			typedef SerialisingSortingBufferedOutputFile<data_type,order_type> this_type;
			typedef typename libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef typename libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;
			typedef libmaus2::sorting::SerialisingMergingReadBack<data_type,order_type> merger_type;
			typedef typename libmaus2::sorting::SerialisingMergingReadBack<data_type,order_type>::unique_ptr_type merger_ptr_type;

			typedef typename libmaus2::util::unique_ptr<order_type>::type order_ptr_type;
			order_ptr_type Porder;
			order_type & order;

			std::string const filename;
			libmaus2::aio::OutputStreamInstance::unique_ptr_type PCOS;
			typename libmaus2::aio::SerialisingSortingBufferedOutput<data_type,order_type>::unique_ptr_type SBO;

			SerialisingSortingBufferedOutputFile(std::string const & rfilename, uint64_t const bufsize = 1024ull)
			: Porder(new order_type), order(*Porder), filename(rfilename), PCOS(new libmaus2::aio::OutputStreamInstance(filename)), SBO(new libmaus2::aio::SerialisingSortingBufferedOutput<data_type,order_type>(*PCOS,bufsize,order))
			{
			}

			SerialisingSortingBufferedOutputFile(std::string const & rfilename, order_type & rorder, uint64_t const bufsize = 1024ull)
			: Porder(), order(rorder), filename(rfilename), PCOS(new libmaus2::aio::OutputStreamInstance(filename)), SBO(new libmaus2::aio::SerialisingSortingBufferedOutput<data_type,order_type>(*PCOS,bufsize,order))
			{
			}

			void put(data_type v)
			{
				SBO->put(v);
			}

			merger_ptr_type getMerger(uint64_t const backblocksize = 1024ull, uint64_t const maxfan = 16ull)
			{
				SBO->flush();
				std::vector< typename libmaus2::aio::SerialisingSortingBufferedOutput<data_type,order_type>::BlockDescriptor > blocksizes = SBO->getBlockSizes();
				SBO.reset();
				PCOS->flush();
				PCOS.reset();

				blocksizes = libmaus2::sorting::SerialisingMergingReadBack<data_type,order_type>::premerge(filename,blocksizes,order,maxfan,backblocksize);

				typename libmaus2::sorting::SerialisingMergingReadBack<data_type,order_type>::unique_ptr_type ptr(
					new libmaus2::sorting::SerialisingMergingReadBack<data_type,order_type>(filename,blocksizes,order,backblocksize)
				);

				return UNIQUE_PTR_MOVE(ptr);
			}

			static void sort(
				std::string const & fn,
				uint64_t const blocksize = 1024ull,
				uint64_t const backblocksize = 1024ull,
				uint64_t const maxfan = 16ull
			)
			{
				std::string const tmpfn = fn + ".tmp";
				libmaus2::util::TempFileRemovalContainer::addTempFile(tmpfn);
				reduce(std::vector<std::string>(1,fn),tmpfn);
				libmaus2::aio::OutputStreamFactoryContainer::rename(tmpfn,fn);
			}

			static void reduce(
				std::vector<std::string> const & Vfn,
				std::string const & out,
				uint64_t const blocksize = 1024ull,
				uint64_t const backblocksize = 1024ull,
				uint64_t const maxfan = 16ull
			)
			{
				std::string const tmp = out + ".tmp";
				libmaus2::util::TempFileRemovalContainer::addTempFile(tmp);
				unique_ptr_type U(new this_type(tmp,blocksize));
				data_type D;

				for ( uint64_t i = 0; i < Vfn.size(); ++i )
				{
					libmaus2::aio::InputStreamInstance ISI(Vfn[i]);
					while ( ISI && ISI.peek() != std::istream::traits_type::eof() )
					{
						D.deserialise(ISI);
						U->put(D);
					}
				}

				merger_ptr_type Pmerger(U->getMerger(backblocksize,maxfan));
				libmaus2::aio::OutputStreamInstance OSI(out);
				while ( Pmerger->getNext(D) )
					D.serialise(OSI);
				OSI.flush();
				Pmerger.reset();
				U.reset();

				libmaus2::aio::FileRemoval::removeFile(tmp);
			}

			static void sortUnique(
				std::string const & fn,
				uint64_t const blocksize = 1024ull,
				uint64_t const backblocksize = 1024ull,
				uint64_t const maxfan = 16ull
			)
			{
				std::string const tmpfn = fn + ".tmp";
				libmaus2::util::TempFileRemovalContainer::addTempFile(tmpfn);
				reduceUnique(std::vector<std::string>(1,fn),tmpfn);
				libmaus2::aio::OutputStreamFactoryContainer::rename(tmpfn,fn);
			}

			static void reduceUnique(
				std::vector<std::string> const & Vfn,
				std::string const & out,
				uint64_t const blocksize = 1024ull,
				uint64_t const backblocksize = 1024ull,
				uint64_t const maxfan = 16ull
			)
			{
				std::string const tmp = out + ".tmp";
				libmaus2::util::TempFileRemovalContainer::addTempFile(tmp);
				unique_ptr_type U(new this_type(tmp,blocksize));
				data_type D;

				for ( uint64_t i = 0; i < Vfn.size(); ++i )
				{
					libmaus2::aio::InputStreamInstance ISI(Vfn[i]);
					while ( ISI && ISI.peek() != std::istream::traits_type::eof() )
					{
						D.deserialise(ISI);
						U->put(D);
					}
				}

				merger_ptr_type Pmerger(U->getMerger(backblocksize,maxfan));
				libmaus2::aio::OutputStreamInstance OSI(out);

				data_type Dprev;
				order_type order;
				if ( Pmerger->getNext(Dprev) )
				{
					while ( Pmerger->getNext(D) )
					{
						if ( order(Dprev,D) )
						{
							Dprev.serialise(OSI);
							Dprev = D;
						}
					}

					Dprev.serialise(OSI);
				}
				OSI.flush();
				Pmerger.reset();
				U.reset();

				libmaus2::aio::FileRemoval::removeFile(tmp);
			}
		};
	}
}
#endif
