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
#if ! defined(LIBMAUS2_GAMMA_SPARSEGAMMAGAPMULTIFILESET_HPP)
#define LIBMAUS2_GAMMA_SPARSEGAMMAGAPMULTIFILESET_HPP

#include <libmaus2/gamma/SparseGammaGapDecoder.hpp>
#include <libmaus2/gamma/SparseGammaGapMultiFile.hpp>
#include <libmaus2/gamma/SparseGammaGapMerge.hpp>
#include <libmaus2/gamma/GammaGapEncoder.hpp>
#include <libmaus2/aio/InputStreamInstance.hpp>
#include <libmaus2/aio/OutputStreamInstance.hpp>
#include <libmaus2/util/TempFileNameGenerator.hpp>
#include <libmaus2/util/TempFileRemovalContainer.hpp>
#include <libmaus2/parallel/OMPLock.hpp>
#include <queue>

namespace libmaus2
{
	namespace gamma
	{
		template<typename _data_type>
		struct SparseGammaGapMultiFileSetTemplate
		{
			typedef _data_type data_type;
			libmaus2::util::TempFileNameGenerator & tmpgen;
			std::priority_queue<libmaus2::gamma::SparseGammaGapMultiFile> Q;
			libmaus2::parallel::OMPLock lock;
			uint64_t addcnt;
			uint64_t parts;

			SparseGammaGapMultiFileSetTemplate(
				libmaus2::util::TempFileNameGenerator & rtmpgen,
				uint64_t rparts
			) : tmpgen(rtmpgen), addcnt(0), parts(rparts) {}

			bool needMerge()
			{
				if ( Q.empty() )
					return false;

				SparseGammaGapMultiFile const Sa = Q.top(); Q.pop();

				if ( Q.empty() )
				{
					Q.push(Sa);
					return false;
				}

				SparseGammaGapMultiFile const Sb = Q.top(); Q.pop();

				bool const needmerge = Sa.level == Sb.level;

				Q.push(Sb);
				Q.push(Sa);

				return needmerge;
			}

			bool canMerge()
			{
				if ( Q.empty() )
					return false;

				SparseGammaGapMultiFile const Sa = Q.top(); Q.pop();

				if ( Q.empty() )
				{
					Q.push(Sa);
					return false;
				}

				SparseGammaGapMultiFile const Sb = Q.top(); Q.pop();

				Q.push(Sb);
				Q.push(Sa);

				return true;
			}

			void doMerge(std::string const & nfn)
			{
				assert ( ! Q.empty() );
				SparseGammaGapMultiFile const Sa = Q.top(); Q.pop();
				assert ( ! Q.empty() );
				SparseGammaGapMultiFile const Sb = Q.top(); Q.pop();

				std::vector<std::string> const fno = libmaus2::gamma::SparseGammaGapMergeTemplate<data_type>::merge(Sa.fn,Sb.fn,nfn,parts,true);
				SparseGammaGapMultiFile N(fno,Sa.level+1);
				Q.push(N);

				#if 0
				std::cerr << "merged " << Sa.fn << " and " << Sb.fn << " to " << nfn << std::endl;
				#endif

				// remove input files
				for ( uint64_t i = 0; i < Sa.fn.size(); ++i )
					libmaus2::aio::FileRemoval::removeFile(Sa.fn[i]);
				for ( uint64_t i = 0; i < Sb.fn.size(); ++i )
					libmaus2::aio::FileRemoval::removeFile(Sb.fn[i]);
			}

			void addFile(std::vector<std::string> const & fn)
			{
				for ( uint64_t i = 0; i < fn.size(); ++i )
					libmaus2::util::TempFileRemovalContainer::addTempFile(fn[i]);

				SparseGammaGapMultiFile S(fn,0);

				libmaus2::parallel::ScopeLock slock(lock);
				addcnt += 1;
				Q.push(S);

				while ( needMerge() )
					doMerge(tmpgen.getFileName());
			}

			void addFile(std::string const & fn)
			{
				addFile(std::vector<std::string>(1,fn));
			}

			std::vector<std::string> merge(std::string const & outputfilenameprefix)
			{
				libmaus2::parallel::ScopeLock slock(lock);

				while ( canMerge() )
					doMerge(tmpgen.getFileName());

				std::vector<std::string> fno;

				if ( !Q.empty() )
				{
					SparseGammaGapMultiFile const S = Q.top();

					for ( uint64_t i = 0; i < S.fn.size(); ++i )
					{
						std::ostringstream ostr;
						ostr << outputfilenameprefix << "_" << std::setw(6) << std::setfill('0') << i;
						std::string const fn = ostr.str();
						libmaus2::aio::OutputStreamFactoryContainer::rename(S.fn[i].c_str(),fn.c_str());
						fno.push_back(fn);
					}
				}

				return fno;
			}

			uint64_t mergeToDense(std::string const & outputfilename, uint64_t const n)
			{
				libmaus2::parallel::ScopeLock slock(lock);
				uint64_t s = 0;

				while ( canMerge() )
					doMerge(tmpgen.getFileName());

				if ( !Q.empty() )
				{
					libmaus2::gamma::SparseGammaGapConcatDecoderTemplate<data_type> SGGD(Q.top().fn);
					typename libmaus2::gamma::SparseGammaGapConcatDecoderTemplate<data_type>::iterator it = SGGD.begin();

					libmaus2::gamma::GammaGapEncoder::unique_ptr_type GGE(new libmaus2::gamma::GammaGapEncoder(outputfilename));
					GGE->encode(it,n);
					GGE.reset();

					{
						libmaus2::gamma::GammaGapDecoder GGD(
							std::vector<std::string>(1,outputfilename),
							0 /* offset */,
							0 /* psymoffset */,
							1 /* numthreads */
						);

						assert ( GGD.getN() == n );

						libmaus2::gamma::SparseGammaGapConcatDecoderTemplate<data_type> SGGD(Q.top().fn);
						typename libmaus2::gamma::SparseGammaGapConcatDecoderTemplate<data_type>::iterator it = SGGD.begin();

						for ( uint64_t i = 0; i < n; ++i )
						{
							uint64_t const v = *(it++);
							uint64_t const vd = GGD.get();
							assert ( v == vd );

							s += v;
						}
					}

					for ( uint64_t i = 0; i < Q.top().fn.size(); ++i )
						libmaus2::aio::FileRemoval::removeFile(Q.top().fn[i]);
				}

				return s;
			}
		};

		typedef SparseGammaGapMultiFileSetTemplate<uint64_t> SparseGammaGapMultiFileSet;
		typedef SparseGammaGapMultiFileSetTemplate< libmaus2::math::UnsignedInteger<4> > SparseGammaGapMultiFileSet2;
	}
}
#endif
