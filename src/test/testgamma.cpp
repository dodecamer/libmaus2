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
#include <libmaus2/gamma/GammaPartitionDecoder.hpp>
#include <libmaus2/gamma/GammaPartitionEncoder.hpp>
#include <libmaus2/gamma/GammaIntervalEncoder.hpp>
#include <libmaus2/gamma/GammaIntervalDecoder.hpp>

#include <libmaus2/gamma/SimpleGammaEncoderArray.hpp>
#include <libmaus2/gamma/SimpleGammaDecoder.hpp>
#include <libmaus2/gamma/SimpleGammaEncoder.hpp>
#include <libmaus2/gamma/GammaEncoder.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <libmaus2/timing/RealTimeClock.hpp>
#include <libmaus2/gamma/GammaDecoder.hpp>

#include <libmaus2/gamma/SparseGammaGapEncoder.hpp>
#include <libmaus2/gamma/SparseGammaGapDecoder.hpp>
#include <libmaus2/gamma/SparseGammaGapMerge.hpp>
#include <libmaus2/gamma/SparseGammaGapFile.hpp>
#include <libmaus2/gamma/SparseGammaGapFileSet.hpp>
#include <libmaus2/gamma/SparseGammaGapFileLevelSet.hpp>

#include <libmaus2/gamma/GammaDifferenceEncoder.hpp>
#include <libmaus2/gamma/GammaDifferenceDecoder.hpp>
#include <libmaus2/gamma/GammaDifferenceMerge.hpp>
#include <libmaus2/gamma/GammaDifferenceOutputBuffer.hpp>

#include <libmaus2/parallel/NumCpus.hpp>

template<typename T>
struct VectorPut : public std::vector<T>
{
	typedef T data_type;

	VectorPut() {}
	void put(T const v)
	{
		std::vector<T>::push_back(v);
	}
};

template<typename _T>
struct CountPut
{
	typedef _T data_type;

	uint64_t cnt;

	CountPut()
	: cnt(0)
	{

	}

	void put(data_type const)
	{
		++cnt;
	}
};

template<typename T>
struct VectorGet
{
	typename std::vector<T>::const_iterator it;
	typedef T data_type;

	VectorGet(typename std::vector<T>::const_iterator rit)
	: it(rit)
	{}

	T get()
	{
		return *(it++);
	}

	bool getNext(T & v)
	{
		v = *(it++);
		return true;
	}
};

void testRandom(unsigned int const n)
{
	srand(time(0));
	std::vector<uint64_t> V(n);
	for ( uint64_t i = 0; i <n; ++i )
		V[i] = rand();

	::libmaus2::timing::RealTimeClock rtc;

	rtc.start();
	CountPut<uint64_t> CP;
	::libmaus2::gamma::GammaEncoder< CountPut<uint64_t> > GCP(CP);
	for ( uint64_t i = 0; i < n; ++i )
		GCP.encode(V[i]);
	GCP.flush();
	double const cencsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] count encoded " << n << " numbers in time " << cencsecs
		<< " rate " << (n / cencsecs)/(1000*1000) << " m/s"
		<< " output words " << CP.cnt
		<< std::endl;

	VectorPut<uint64_t> VP;
	rtc.start();
	::libmaus2::gamma::GammaEncoder< VectorPut<uint64_t> > GE(VP);
	for ( uint64_t i = 0; i < n; ++i )
		GE.encode(V[i]);
	GE.flush();
	double const encsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] encoded " << n << " numbers to dyn growing vector in time " << encsecs
		<< " rate " << (n / encsecs)/(1000*1000) << " m/s"
		<< std::endl;

	rtc.start();
	VectorGet<uint64_t> VG(VP.begin());
	::libmaus2::gamma::GammaDecoder < VectorGet<uint64_t> > GD(VG);
	bool ok = true;
	for ( uint64_t i = 0; ok && i < n; ++i )
	{
		uint64_t const v = GD.decode();
		ok = ok && (v==V[i]);
		if ( ! ok )
		{
			std::cerr << "expected " << V[i] << " got " << v << std::endl;
		}
	}
	double const decsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] decoded " << n << " numbers in time " << decsecs
		<< " rate " << (n / decsecs)/(1000*1000) << " m/s"
		<< std::endl;

	if ( ok )
	{
		std::cout << "Test of gamma coding with " << n << " random numbers ok." << std::endl;
	}
	else
	{
		std::cout << "Test of gamma coding with " << n << " random numbers failed." << std::endl;
	}
}

void testRandom2(unsigned int const n)
{
	srand(time(0));
	std::vector< libmaus2::math::UnsignedInteger<4> > V(n);
	for ( uint64_t i = 0; i <n; ++i )
		V[i] = libmaus2::math::UnsignedInteger<4>(rand());

	::libmaus2::timing::RealTimeClock rtc;

	rtc.start();
	CountPut< libmaus2::math::UnsignedInteger<4> > CP;
	::libmaus2::gamma::GammaEncoder< CountPut< libmaus2::math::UnsignedInteger<4> > > GCP(CP);
	for ( uint64_t i = 0; i < n; ++i )
		GCP.encode(V[i]);
	GCP.flush();
	double const cencsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] count2 encoded " << n << " numbers in time " << cencsecs
		<< " rate " << (n / cencsecs)/(1000*1000) << " m/s"
		<< " output words " << CP.cnt
		<< std::endl;

	VectorPut< libmaus2::math::UnsignedInteger<4> > VP;
	rtc.start();
	::libmaus2::gamma::GammaEncoder< VectorPut< libmaus2::math::UnsignedInteger<4> > > GE(VP);
	for ( uint64_t i = 0; i < n; ++i )
		GE.encode(V[i]);
	GE.flush();
	double const encsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] encoded2 " << n << " numbers to dyn growing vector in time " << encsecs
		<< " rate " << (n / encsecs)/(1000*1000) << " m/s"
		<< std::endl;

	rtc.start();
	VectorGet< libmaus2::math::UnsignedInteger<4> > VG(VP.begin());
	::libmaus2::gamma::GammaDecoder < VectorGet< libmaus2::math::UnsignedInteger<4> > > GD(VG);
	bool ok = true;
	for ( uint64_t i = 0; ok && i < n; ++i )
	{
		libmaus2::math::UnsignedInteger<4> const v = GD.decode();
		ok = ok && (v==V[i]);
		if ( ! ok )
		{
			std::cerr << "expected " << V[i] << " got " << v << std::endl;
		}
	}
	double const decsecs = rtc.getElapsedSeconds();
	std::cerr << "[V] decoded2 " << n << " numbers in time " << decsecs
		<< " rate " << (n / decsecs)/(1000*1000) << " m/s"
		<< std::endl;

	if ( ok )
	{
		std::cout << "Test2 of gamma coding with " << n << " random numbers ok." << std::endl;
	}
	else
	{
		std::cout << "Test2 of gamma coding with " << n << " random numbers failed." << std::endl;
	}
}

void testLow()
{
	unsigned int const n = 127;
	VectorPut<uint64_t> VP;
	::libmaus2::gamma::GammaEncoder< VectorPut<uint64_t> > GE(VP);
	for ( uint64_t i = 0; i < n; ++i )
		GE.encode(i);
	GE.flush();

	VectorGet<uint64_t> VG(VP.begin());
	::libmaus2::gamma::GammaDecoder < VectorGet<uint64_t> > GD(VG);
	bool ok = true;
	for ( uint64_t i = 0; ok && i < n; ++i )
		ok = ok && ( GD.decode() == i );

	if ( ok )
	{
		std::cout << "Test of gamma coding with numbers up to 126 ok." << std::endl;
	}
	else
	{
		std::cout << "Test of gamma coding with numbers up to 126 failed." << std::endl;
	}
}

void testLow2()
{
	unsigned int const n = 127;
	VectorPut< libmaus2::math::UnsignedInteger<4> > VP;
	::libmaus2::gamma::GammaEncoder< VectorPut< libmaus2::math::UnsignedInteger<4> > > GE(VP);
	for ( uint64_t i = 0; i < n; ++i )
		GE.encode(i);
	GE.flush();

	VectorGet< libmaus2::math::UnsignedInteger<4>  > VG(VP.begin());
	::libmaus2::gamma::GammaDecoder < VectorGet< libmaus2::math::UnsignedInteger<4>  > > GD(VG);
	bool ok = true;
	for ( uint64_t i = 0; ok && i < n; ++i )
		ok = ok && ( GD.decode() == i );

	if ( ok )
	{
		std::cout << "Test of gamma coding with numbers up to 126 ok." << std::endl;
	}
	else
	{
		std::cout << "Test of gamma coding with numbers up to 126 failed." << std::endl;
	}
}

#include <libmaus2/gamma/GammaGapEncoder.hpp>
#include <libmaus2/huffman/IndexDecoderDataArray.hpp>
#include <libmaus2/gamma/GammaGapDecoder.hpp>
#include <libmaus2/util/TempFileRemovalContainer.hpp>

void testgammagap()
{
	unsigned int n = 512*1024+199481101;
	std::vector<uint64_t> V (n);
	std::vector<uint64_t> V2(n);
	for ( uint64_t i = 0; i < V.size(); ++i )
	{
		V[i] = i & 0xFFull;
		V2[i] = rand() % 0xFFull;
	}

	std::string const fn("tmpfile");
	std::string const fn2("tmpfile2");
	std::string const fnm("tmpfile.merged");

	::libmaus2::util::TempFileRemovalContainer::setup();
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fn2);
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fnm);

	::libmaus2::gamma::GammaGapEncoder GGE(fn);
	GGE.encode(V.begin(),V.end());
	::libmaus2::gamma::GammaGapEncoder GGE2(fn2);
	GGE2.encode(V2.begin(),V2.end());

	::libmaus2::huffman::IndexDecoderData IDD(fn);

	::libmaus2::gamma::GammaGapDecoder GGD(std::vector<std::string>(1,fn), 0/* offset */,0 /* psymoff */,1 /* numthreads */);

	bool ok = true;
	for ( uint64_t i = 0; i < n; ++i )
	{
		uint64_t const v = GGD.decode();
		ok = ok && (v == V[i]);
	}
	std::cout << "decoding " << (ok ? "ok" : "fail") << std::endl;

	std::vector < std::vector<std::string> > merin;
	merin.push_back(std::vector<std::string>(1,fn));
	merin.push_back(std::vector<std::string>(1,fn2));
	::libmaus2::gamma::GammaGapEncoder::merge(merin,fnm,1 /* numthreads */);
}

#include <libmaus2/gamma/GammaRLEncoder.hpp>
#include <libmaus2/gamma/GammaRLDecoder.hpp>

void testgammarl()
{
	srand(time(0));
	unsigned int n = 128*1024*1024;
	unsigned int n2 = 64*1024*1024;
	std::vector<uint64_t> V (n);
	std::vector<uint64_t> V2 (n2);
	std::vector<uint64_t> Vcat;
	unsigned int const albits = 3;
	uint64_t const almask = (1ull << albits)-1;

	for ( uint64_t i = 0; i < V.size(); ++i )
	{
		V[i]  = rand() & almask;
		Vcat.push_back(V[i]);
	}
	for ( uint64_t i = 0; i < V2.size(); ++i )
	{
		V2[i] = rand() & almask;
		Vcat.push_back(V2[i]);
	}

	std::string const fn("tmpfile");
	std::string const fn2("tmpfile2");
	std::string const fn3("tmpfile3");
	::libmaus2::util::TempFileRemovalContainer::setup();
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fn);
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fn2);
	::libmaus2::util::TempFileRemovalContainer::addTempFile(fn3);

	::libmaus2::gamma::GammaRLEncoder GE(fn,albits,V.size(),256*1024);
	for ( uint64_t i = 0; i < V.size(); ++i )
		GE.encode(V[i]);
	GE.flush();

	::libmaus2::gamma::GammaRLEncoder GE2(fn2,albits,V2.size(),256*1024);
	for ( uint64_t i = 0; i < V2.size(); ++i )
		GE2.encode(V2[i]);
	GE2.flush();

	#if 0
	::libmaus2::huffman::IndexDecoderData IDD(fn);
	for ( uint64_t i = 0; i < IDD.numentries+1; ++i )
		std::cerr << IDD.readEntry(i) << std::endl;
	#endif

	::libmaus2::gamma::GammaRLDecoder GD(std::vector<std::string>(1,fn),0 /* offset */,64*1024 /* bufsize */,1/* numthreads */);
	assert ( GD.getN() == n );

	for ( uint64_t i = 0; i < n; ++i )
		assert ( GD.decode() == static_cast<int64_t>(V[i]) );

	uint64_t const off = n / 2 + 1031;
	::libmaus2::gamma::GammaRLDecoder GD2(std::vector<std::string>(1,fn),off,64*1024 /* bufsize */,1 /* numthreads */);
	for ( uint64_t i = off; i < n; ++i )
		assert ( GD2.decode() == static_cast<int64_t>(V[i]) );

	std::vector<std::string> fnv;
	fnv.push_back(fn);
	fnv.push_back(fn2);
	::libmaus2::gamma::GammaRLEncoder::concatenate(fnv,fn3);

	for ( uint64_t off = 0; off < Vcat.size(); off += 18521 )
	{
		::libmaus2::gamma::GammaRLDecoder GD3(std::vector<std::string>(1,fn3),off,64*1024 /* bufsize */,1/*numthreads */);
		assert ( GD3.getN() == Vcat.size() );

		for ( uint64_t i = 0; i < std::min(static_cast<uint64_t>(1024ull),Vcat.size()-off); ++i )
			assert ( GD3.decode() == static_cast<int64_t>(Vcat[off+i]) );
	}

	libmaus2::aio::FileRemoval::removeFile ( fn );
	libmaus2::aio::FileRemoval::removeFile ( fn2 );
	libmaus2::aio::FileRemoval::removeFile ( fn3 );
}

void testgammasparse()
{
	std::ostringstream o0;
	libmaus2::gamma::SparseGammaGapEncoder SE0(o0);
	std::ostringstream o1;
	libmaus2::gamma::SparseGammaGapEncoder SE1(o1);

	SE0.encode(4, 7);
	SE0.encode(6, 3);
	SE0.term();

	SE1.encode(0, 1);
	SE1.encode(2, 5);
	SE1.encode(6, 2);
	SE1.encode(8, 7);
	SE1.term();

	std::cerr << "o0.size()=" << o0.str().size() << std::endl;
	std::cerr << "o1.size()=" << o1.str().size() << std::endl;

	std::istringstream i0(o0.str());
	libmaus2::gamma::SparseGammaGapDecoder SD0(i0);
	std::istringstream i1(o1.str());
	libmaus2::gamma::SparseGammaGapDecoder SD1(i1);

	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SD0.decode() << ";";
	std::cerr << std::endl;
	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SD1.decode() << ";";
	std::cerr << std::endl;

	std::istringstream mi0(o0.str());
	std::istringstream mi1(o1.str());
	std::ostringstream mo;

	libmaus2::gamma::SparseGammaGapMerge::merge(mi0,mi1,mo);

	std::istringstream mi(mo.str());
	libmaus2::gamma::SparseGammaGapDecoder SDM(mi);

	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SDM.decode() << ";";
	std::cerr << std::endl;
}

void testgammasparse2()
{
	std::ostringstream o0;
	libmaus2::gamma::SparseGammaGapEncoder2 SE0(o0);
	std::ostringstream o1;
	libmaus2::gamma::SparseGammaGapEncoder2 SE1(o1);

	SE0.encode(4, 7);
	SE0.encode(6, 3);
	SE0.term();

	SE1.encode(0, 1);
	SE1.encode(2, 5);
	SE1.encode(6, 2);
	SE1.encode(8, 7);
	SE1.term();

	std::cerr << "o0.size()=" << o0.str().size() << std::endl;
	std::cerr << "o1.size()=" << o1.str().size() << std::endl;

	std::istringstream i0(o0.str());
	libmaus2::gamma::SparseGammaGapDecoder2 SD0(i0);
	std::istringstream i1(o1.str());
	libmaus2::gamma::SparseGammaGapDecoder2 SD1(i1);

	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SD0.decode() << ";";
	std::cerr << std::endl;
	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SD1.decode() << ";";
	std::cerr << std::endl;

	std::istringstream mi0(o0.str());
	std::istringstream mi1(o1.str());
	std::ostringstream mo;

	libmaus2::gamma::SparseGammaGapMerge2::merge(mi0,mi1,mo);

	std::istringstream mi(mo.str());
	libmaus2::gamma::SparseGammaGapDecoder2 SDM(mi);

	for ( uint64_t i = 0; i < 10; ++i )
		std::cerr << SDM.decode() << ";";
	std::cerr << std::endl;
}


void testsparsegammalevelmerge()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapFileLevelSet SGGF(tmpgen);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::gamma::SparseGammaGapEncoder SGE(COS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);
	}

	std::string const ffn = tmpgen.getFileName();
	SGGF.merge(ffn);

	libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapDecoder SGGD(CIS);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	libmaus2::aio::FileRemoval::removeFile(ffn);
}

void testsparsegammalevelmerge2()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapFileLevelSet2 SGGF(tmpgen);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::gamma::SparseGammaGapEncoder2 SGE(COS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);
	}

	std::string const ffn = tmpgen.getFileName();
	SGGF.merge(ffn);

	libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapDecoder2 SGGD(CIS);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	libmaus2::aio::FileRemoval::removeFile(ffn);
}


#include <libmaus2/gamma/SparseGammaGapBlockEncoder.hpp>
#include <libmaus2/gamma/SparseGammaGapConcatDecoder.hpp>

void testSparseGammaConcat()
{
	for ( uint64_t An = 1; An <= 512; ++An )
		for ( uint64_t z = 0; z < 4; ++z )
		{
			// set up array with random numbers
			uint64_t const Amod = (An+1)/2 + (rand() % An);
			libmaus2::autoarray::AutoArray<uint64_t> A(An,false);
			for ( uint64_t i = 0; i < A.size(); ++i )
				A[i] = rand() % Amod;

			// file name prefix
			std::string const fnpref = "mem://tmp_g";

			// encode array
			std::vector<std::string> concfn = libmaus2::gamma::SparseGammaGapBlockEncoder::encodeArray(
				&A[0], &A[An], fnpref, 7 /* parts */, 5 /* block size */);

			// store data in map for reference
			std::map<uint64_t,uint64_t> M;
			for ( uint64_t i = 0; i < An; ++i )
				M[A[i]]++;

			uint64_t maxv = 0;
			std::vector<uint64_t> const splitkeys = libmaus2::gamma::SparseGammaGapFileIndexMultiDecoder::getSplitKeys(
				concfn,concfn,/* Amod, */8,maxv);

			#if 0
			std::cerr << "splitkeys: ";
			for ( uint64_t i = 0; i < splitkeys.size(); ++i )
				std::cerr << splitkeys[i] << ";";
			std::cerr << std::endl;

			for ( std::map<uint64_t,uint64_t>::const_iterator ita = M.begin(); ita != M.end(); ++ita )
				std::cerr << "(k" << ita->first << ",v=" << ita->second << ")";
			std::cerr << std::endl;
			#endif

			// test reading back starting from beginning
			libmaus2::gamma::SparseGammaGapFileIndexMultiDecoder concindex(concfn);
			libmaus2::gamma::SparseGammaGapConcatDecoder SGGCD(concfn);
			int64_t prevkey = -1;
			for ( uint64_t i = 0; i < Amod; ++i )
			{
				uint64_t const v = SGGCD.decode();
				// std::cerr << i << "\t" << v << "\t" << M[i] << std::endl;
				assert ( v == M[i] );

				bool const prevok = ( prevkey == libmaus2::gamma::SparseGammaGapConcatDecoder::getPrevKey(concindex,i) );

				#if 0
				if ( ! prevok )
				{
					std::cerr << "expected " << prevkey << " got " << libmaus2::gamma::SparseGammaGapConcatDecoder::getPrevKey(concindex,i) << std::endl;
				}
				#endif

				assert ( prevok );

				if ( v )
					prevkey = i;
			}

			// test reading back from given starting position
			for ( uint64_t j = 0; j < Amod; ++j )
			{
				// std::cerr << "*** j=" << j << " ***" << std::endl;
				libmaus2::gamma::SparseGammaGapConcatDecoder SGGCD(concfn,j);

				for ( uint64_t i = j; i < Amod; ++i )
				{
					uint64_t const v = SGGCD.decode();
					if ( v != M[i] )
						std::cerr << j << "\t" << i << "\t" << v << "\t" << M[i] << std::endl;
					assert ( v == M[i] );
				}
			}

			// remove files
			for ( uint64_t i = 0; i < concfn.size(); ++i )
				libmaus2::aio::FileRemoval::removeFile(concfn[i]);

			std::cerr << An << "\t" << z+1 << std::endl;
		}
}

void testSparseGammaConcat2()
{
	for ( uint64_t An = 1; An <= 512; ++An )
		for ( uint64_t z = 0; z < 4; ++z )
		{
			// set up array with random numbers
			uint64_t const Amod = (An+1)/2 + (rand() % An);
			libmaus2::autoarray::AutoArray<uint64_t> A(An,false);
			for ( uint64_t i = 0; i < A.size(); ++i )
				A[i] = rand() % Amod;

			// file name prefix
			std::string const fnpref = "mem://tmp_g";

			// encode array
			std::vector<std::string> concfn = libmaus2::gamma::SparseGammaGapBlockEncoder2::encodeArray(
				&A[0], &A[An], fnpref, 7 /* parts */, 5 /* block size */);

			// store data in map for reference
			std::map<uint64_t,uint64_t> M;
			for ( uint64_t i = 0; i < An; ++i )
				M[A[i]]++;

			uint64_t maxv = 0;
			std::vector<uint64_t> const splitkeys = libmaus2::gamma::SparseGammaGapFileIndexMultiDecoder::getSplitKeys(
				concfn,concfn,/* Amod, */8,maxv);

			#if 0
			std::cerr << "splitkeys: ";
			for ( uint64_t i = 0; i < splitkeys.size(); ++i )
				std::cerr << splitkeys[i] << ";";
			std::cerr << std::endl;

			for ( std::map<uint64_t,uint64_t>::const_iterator ita = M.begin(); ita != M.end(); ++ita )
				std::cerr << "(k" << ita->first << ",v=" << ita->second << ")";
			std::cerr << std::endl;
			#endif

			// test reading back starting from beginning
			libmaus2::gamma::SparseGammaGapFileIndexMultiDecoder concindex(concfn);
			libmaus2::gamma::SparseGammaGapConcatDecoder2 SGGCD(concfn);
			int64_t prevkey = -1;
			for ( uint64_t i = 0; i < Amod; ++i )
			{
				uint64_t const v = SGGCD.decode();
				// std::cerr << i << "\t" << v << "\t" << M[i] << std::endl;
				assert ( v == M[i] );

				bool const prevok = ( prevkey == libmaus2::gamma::SparseGammaGapConcatDecoder2::getPrevKey(concindex,i) );

				#if 0
				if ( ! prevok )
				{
					std::cerr << "expected " << prevkey << " got " << libmaus2::gamma::SparseGammaGapConcatDecoder2::getPrevKey(concindex,i) << std::endl;
				}
				#endif

				assert ( prevok );

				if ( v )
					prevkey = i;
			}

			// test reading back from given starting position
			for ( uint64_t j = 0; j < Amod; ++j )
			{
				// std::cerr << "*** j=" << j << " ***" << std::endl;
				libmaus2::gamma::SparseGammaGapConcatDecoder2 SGGCD(concfn,j);

				for ( uint64_t i = j; i < Amod; ++i )
				{
					uint64_t const v = SGGCD.decode();
					if ( v != M[i] )
						std::cerr << j << "\t" << i << "\t" << v << "\t" << M[i] << std::endl;
					assert ( v == M[i] );
				}
			}

			// remove files
			for ( uint64_t i = 0; i < concfn.size(); ++i )
				libmaus2::aio::FileRemoval::removeFile(concfn[i]);

			std::cerr << An << "\t" << z+1 << std::endl;
		}
}

void testSparseGammaIndexing()
{
	std::stringstream datastr;
	std::stringstream indexstr;
	libmaus2::gamma::SparseGammaGapBlockEncoder SGGBE(datastr,indexstr,-1 /* prevkey */,2);
	SGGBE.encode(2,3);
	SGGBE.encode(7,2);
	SGGBE.encode(11,1);
	SGGBE.encode(12,5);
	SGGBE.encode(13,4);
	SGGBE.term();

	std::istringstream istr(datastr.str());
	libmaus2::gamma::SparseGammaGapFileIndexDecoder SGGFID(istr);

	std::cerr << SGGFID;

	std::cerr << std::string(80,'-') << std::endl;

	// std::cerr << SGGFID.getBlockIndex(1) << std::endl;
	std::cerr << SGGFID.getBlockIndex(2) << std::endl;
	std::cerr << SGGFID.getBlockIndex(7) << std::endl;
	std::cerr << SGGFID.getBlockIndex(11) << std::endl;
	std::cerr << SGGFID.getBlockIndex(12) << std::endl;
	std::cerr << SGGFID.getBlockIndex(13) << std::endl;
	std::cerr << SGGFID.getBlockIndex(512) << std::endl;
}

void testSparseGammaIndexing2()
{
	std::stringstream datastr;
	std::stringstream indexstr;
	libmaus2::gamma::SparseGammaGapBlockEncoder2 SGGBE(datastr,indexstr,-1 /* prevkey */,2);
	SGGBE.encode(2,3);
	SGGBE.encode(7,2);
	SGGBE.encode(11,1);
	SGGBE.encode(12,5);
	SGGBE.encode(13,4);
	SGGBE.term();

	std::istringstream istr(datastr.str());
	libmaus2::gamma::SparseGammaGapFileIndexDecoder SGGFID(istr);

	std::cerr << SGGFID;

	std::cerr << std::string(80,'-') << std::endl;

	// std::cerr << SGGFID.getBlockIndex(1) << std::endl;
	std::cerr << SGGFID.getBlockIndex(2) << std::endl;
	std::cerr << SGGFID.getBlockIndex(7) << std::endl;
	std::cerr << SGGFID.getBlockIndex(11) << std::endl;
	std::cerr << SGGFID.getBlockIndex(12) << std::endl;
	std::cerr << SGGFID.getBlockIndex(13) << std::endl;
	std::cerr << SGGFID.getBlockIndex(512) << std::endl;
}


void testSparseGammaGapMergingSmall(uint64_t * A, uint64_t const An, uint64_t * B, uint64_t const Bn)
{
	// file name prefix
	std::string const fnprefa = "mem://tmp_a";
	std::string const fnprefb = "mem://tmp_b";
	std::string const fnout = "mem://tmp_o";

	// encode array
	std::vector<std::string> concafn = libmaus2::gamma::SparseGammaGapBlockEncoder::encodeArray(&A[0], &A[An], fnprefa, 7 /* parts */, 2 /* block size */);
	std::vector<std::string> concbfn = libmaus2::gamma::SparseGammaGapBlockEncoder::encodeArray(&B[0], &B[Bn], fnprefb, 7 /* parts */, 2 /* block size */);

	// libmaus2::gamma::SparseGammaGapMerge::merge(concafn,concbfn,0,std::numeric_limits<uint64_t>::max(),fnout);
	std::vector<std::string> const concofn = libmaus2::gamma::SparseGammaGapMerge::merge(concafn,concbfn,fnout,10);

	#if 0
	for ( uint64_t i = 0; i < concafn.size(); ++i )
		std::cerr << "concafn[i]=" << concafn[i] << std::endl;
	for ( uint64_t i = 0; i < concbfn.size(); ++i )
		std::cerr << "concbfn[i]=" << concbfn[i] << std::endl;
	for ( uint64_t i = 0; i < concofn.size(); ++i )
		std::cerr << "concofn[i]=" << concofn[i] << std::endl;
	#endif

	std::map<uint64_t,uint64_t> M;
	for ( uint64_t i = 0; i < An; ++i ) M[A[i]]++;
	for ( uint64_t i = 0; i < Bn; ++i ) M[B[i]]++;
	uint64_t const maxv = M.size() ? M.rbegin()->first : 0;

	libmaus2::gamma::SparseGammaGapConcatDecoder dec(concofn);

	for ( uint64_t i = 0; i <= maxv; ++i )
	{
		uint64_t const v = dec.decode();
		uint64_t const Mi = (M.find(i) != M.end()) ? M.find(i)->second : 0;

		if ( v != Mi )
			std::cerr << i << "\t" << v << "\t" << Mi << std::endl;

		assert ( v == Mi );
	}

	// remove files
	for ( uint64_t i = 0; i < concafn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concafn[i]);
	for ( uint64_t i = 0; i < concbfn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concbfn[i]);
	for ( uint64_t i = 0; i < concofn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concofn[i]);
}

void testSparseGammaGapMergingSmall2(uint64_t * A, uint64_t const An, uint64_t * B, uint64_t const Bn)
{
	// file name prefix
	std::string const fnprefa = "mem://tmp_a";
	std::string const fnprefb = "mem://tmp_b";
	std::string const fnout = "mem://tmp_o";

	// encode array
	std::vector<std::string> concafn = libmaus2::gamma::SparseGammaGapBlockEncoder2::encodeArray(&A[0], &A[An], fnprefa, 7 /* parts */, 2 /* block size */);
	std::vector<std::string> concbfn = libmaus2::gamma::SparseGammaGapBlockEncoder2::encodeArray(&B[0], &B[Bn], fnprefb, 7 /* parts */, 2 /* block size */);

	// libmaus2::gamma::SparseGammaGapMerge::merge(concafn,concbfn,0,std::numeric_limits<uint64_t>::max(),fnout);
	std::vector<std::string> const concofn = libmaus2::gamma::SparseGammaGapMerge2::merge(concafn,concbfn,fnout,10);

	#if 0
	for ( uint64_t i = 0; i < concafn.size(); ++i )
		std::cerr << "concafn[i]=" << concafn[i] << std::endl;
	for ( uint64_t i = 0; i < concbfn.size(); ++i )
		std::cerr << "concbfn[i]=" << concbfn[i] << std::endl;
	for ( uint64_t i = 0; i < concofn.size(); ++i )
		std::cerr << "concofn[i]=" << concofn[i] << std::endl;
	#endif

	std::map<uint64_t,uint64_t> M;
	for ( uint64_t i = 0; i < An; ++i ) M[A[i]]++;
	for ( uint64_t i = 0; i < Bn; ++i ) M[B[i]]++;
	uint64_t const maxv = M.size() ? M.rbegin()->first : 0;

	libmaus2::gamma::SparseGammaGapConcatDecoder2 dec(concofn);

	for ( uint64_t i = 0; i <= maxv; ++i )
	{
		uint64_t const v = dec.decode();
		uint64_t const Mi = (M.find(i) != M.end()) ? M.find(i)->second : 0;

		if ( v != Mi )
			std::cerr << i << "\t" << v << "\t" << Mi << std::endl;

		assert ( v == Mi );
	}

	// remove files
	for ( uint64_t i = 0; i < concafn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concafn[i]);
	for ( uint64_t i = 0; i < concbfn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concbfn[i]);
	for ( uint64_t i = 0; i < concofn.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(concofn[i]);
}

libmaus2::autoarray::AutoArray<uint64_t> randomArray(uint64_t const n, uint64_t const klow, uint64_t const khigh)
{
	if ( ! n )
		return libmaus2::autoarray::AutoArray<uint64_t>(0);

	assert ( khigh-klow );

	libmaus2::autoarray::AutoArray<uint64_t> A(n,false);

	for ( uint64_t i = 0; i < n; ++i )
		A[i] = klow + ( libmaus2::random::Random::rand64() % (khigh-klow) );

	return A;
}

void testSparseGammaGapMergingRandom(
	uint64_t const nl,
	uint64_t const kll,
	uint64_t const klh,
	uint64_t const nr,
	uint64_t const krl,
	uint64_t const krh
)
{
	libmaus2::autoarray::AutoArray<uint64_t> Al = randomArray(nl,kll,klh);
	libmaus2::autoarray::AutoArray<uint64_t> Ar = randomArray(nr,krl,krh);
	testSparseGammaGapMergingSmall(Al.begin(),Al.size(),Ar.begin(),Ar.size());
}

void testSparseGammaGapMergingRandom2(
	uint64_t const nl,
	uint64_t const kll,
	uint64_t const klh,
	uint64_t const nr,
	uint64_t const krl,
	uint64_t const krh
)
{
	libmaus2::autoarray::AutoArray<uint64_t> Al = randomArray(nl,kll,klh);
	libmaus2::autoarray::AutoArray<uint64_t> Ar = randomArray(nr,krl,krh);
	testSparseGammaGapMergingSmall2(Al.begin(),Al.size(),Ar.begin(),Ar.size());
}

void testSparseGammaGapMergingRandom()
{
	// test full overlapping
	uint64_t const loops = 32;
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom(nl,k0,k1,nr,k0,k1);
		testSparseGammaGapMergingRandom(nr,k0,k1,nr,k0,k1);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << ")";
	}
	std::cerr << std::endl;

	// test part overlapping
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const ksl = k1-(k1-k0)/2;
		uint64_t const k2 = k1 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const ksr = k2-(k2-k1)/2;
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom(nl,k0,ksl,nr,ksr,k2);
		testSparseGammaGapMergingRandom(nr,ksr,k2,nl,k0,ksl);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << "," << k2 << ")";
	}
	std::cerr << std::endl;

	// test non overlapping
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const k2 = k1 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom(nl,k0,k1,nr,k1,k2);
		testSparseGammaGapMergingRandom(nr,k1,k2,nl,k0,k1);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << "," << k2 << ")";
	}
	std::cerr << std::endl;

}

void testSparseGammaGapMergingRandom2()
{
	// test full overlapping
	uint64_t const loops = 32;
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom2(nl,k0,k1,nr,k0,k1);
		testSparseGammaGapMergingRandom2(nr,k0,k1,nr,k0,k1);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << ")";
	}
	std::cerr << std::endl;

	// test part overlapping
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const ksl = k1-(k1-k0)/2;
		uint64_t const k2 = k1 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const ksr = k2-(k2-k1)/2;
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom2(nl,k0,ksl,nr,ksr,k2);
		testSparseGammaGapMergingRandom2(nr,ksr,k2,nl,k0,ksl);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << "," << k2 << ")";
	}
	std::cerr << std::endl;

	// test non overlapping
	for ( uint64_t i = 0; i < loops; ++i )
	{
		uint64_t const k0 = libmaus2::random::Random::rand64() % 1024;
		uint64_t const k1 = k0 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const k2 = k1 + (libmaus2::random::Random::rand64() % 1024+1);
		uint64_t const nl = libmaus2::random::Random::rand64() % 1024;
		uint64_t const nr = libmaus2::random::Random::rand64() % 1024;

		testSparseGammaGapMergingRandom2(nl,k0,k1,nr,k1,k2);
		testSparseGammaGapMergingRandom2(nr,k1,k2,nl,k0,k1);

		std::cerr << "(" << nl << "," << nr << "," << k0 << "," << k1 << "," << k2 << ")";
	}
	std::cerr << std::endl;
}

void testSparseGammaGapMergingSmall()
{
	uint64_t const AZ[] = { 1,6,1,7,21,1,6,6,7,4,42,14,16,25,28,100,83,70,75 };
	uint64_t const A1[] = { 1,3,50,52,75,77,1000,1002,2000,3000 };
	uint64_t const A2[] = { 11,13,60,62,82,83 };
	uint64_t const A3[] = { };
	uint64_t const *A[] = { &AZ[0], &A1[0], &A2[0], &A3[0], 0 };
	uint64_t const An[] = { sizeof(AZ)/sizeof(AZ[0]), sizeof(A1)/sizeof(A1[0]), sizeof(A2)/sizeof(A2[0]), sizeof(A3)/sizeof(A3[0]), 0 };

	uint64_t const BZ[] = { 1,6,1,7,21,1,6,6,7,5,43,13,18,24,29,95,86,72,77 };
	uint64_t const B1[] = { 11,13,60,62,82,83 };
	uint64_t const B2[] = { 1000,1002,1004,1006,1008,1010,1012, 1014, 1017, 1020 };
	uint64_t const B3[] = { };
	uint64_t const B4[] = { 1,6,1,7,21,1,6,6,7,4,42,14,16,25,28,100,83,70,75 };
	uint64_t const *B[] = { &BZ[0], &B1[0], &B2[2], &B3[0], &B4[0], 0 };
	uint64_t const Bn[] = { sizeof(BZ)/sizeof(BZ[0]), sizeof(B1)/sizeof(B1[0]), sizeof(B2)/sizeof(B2[0]), sizeof(B3)/sizeof(B3[0]), sizeof(B4)/sizeof(B4[0]), 0 };

	for ( uint64_t a = 0; A[a]; ++a )
		for ( uint64_t b = 0; B[b]; ++b )
		{
			libmaus2::autoarray::AutoArray<uint64_t> At(An[a],false);
			libmaus2::autoarray::AutoArray<uint64_t> Bt(Bn[b],false);
			std::copy(A[a],A[a]+An[a],At.begin());
			std::copy(B[b],B[b]+Bn[b],Bt.begin());
			testSparseGammaGapMergingSmall(At.begin(),An[a],Bt.begin(),Bn[b]);
		}
}

void testSparseGammaGapMergingSmall2()
{
	uint64_t const AZ[] = { 1,6,1,7,21,1,6,6,7,4,42,14,16,25,28,100,83,70,75 };
	uint64_t const A1[] = { 1,3,50,52,75,77,1000,1002,2000,3000 };
	uint64_t const A2[] = { 11,13,60,62,82,83 };
	uint64_t const A3[] = { };
	uint64_t const *A[] = { &AZ[0], &A1[0], &A2[0], &A3[0], 0 };
	uint64_t const An[] = { sizeof(AZ)/sizeof(AZ[0]), sizeof(A1)/sizeof(A1[0]), sizeof(A2)/sizeof(A2[0]), sizeof(A3)/sizeof(A3[0]), 0 };

	uint64_t const BZ[] = { 1,6,1,7,21,1,6,6,7,5,43,13,18,24,29,95,86,72,77 };
	uint64_t const B1[] = { 11,13,60,62,82,83 };
	uint64_t const B2[] = { 1000,1002,1004,1006,1008,1010,1012, 1014, 1017, 1020 };
	uint64_t const B3[] = { };
	uint64_t const B4[] = { 1,6,1,7,21,1,6,6,7,4,42,14,16,25,28,100,83,70,75 };
	uint64_t const *B[] = { &BZ[0], &B1[0], &B2[2], &B3[0], &B4[0], 0 };
	uint64_t const Bn[] = { sizeof(BZ)/sizeof(BZ[0]), sizeof(B1)/sizeof(B1[0]), sizeof(B2)/sizeof(B2[0]), sizeof(B3)/sizeof(B3[0]), sizeof(B4)/sizeof(B4[0]), 0 };

	for ( uint64_t a = 0; A[a]; ++a )
		for ( uint64_t b = 0; B[b]; ++b )
		{
			libmaus2::autoarray::AutoArray<uint64_t> At(An[a],false);
			libmaus2::autoarray::AutoArray<uint64_t> Bt(Bn[b],false);
			std::copy(A[a],A[a]+An[a],At.begin());
			std::copy(B[b],B[b]+Bn[b],Bt.begin());
			testSparseGammaGapMergingSmall2(At.begin(),An[a],Bt.begin(),Bn[b]);
		}
}

#include <libmaus2/gamma/SparseGammaGapMultiFileSet.hpp>

void testsparsegammamerge()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapFileSet SGGF(tmpgen);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::gamma::SparseGammaGapEncoder SGE(COS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);
	}

	std::string const ffn = tmpgen.getFileName();
	SGGF.merge(ffn);

	libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapDecoder SGGD(CIS);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	libmaus2::aio::FileRemoval::removeFile(ffn);
}

void testsparsegammamerge2()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapFileSet2 SGGF(tmpgen);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::gamma::SparseGammaGapEncoder2 SGE(COS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);
	}

	std::string const ffn = tmpgen.getFileName();
	SGGF.merge(ffn);

	libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapDecoder2 SGGD(CIS);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	libmaus2::aio::FileRemoval::removeFile(ffn);
}


void testsparsegammamultimerge()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileSet SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	std::string const ffn = tmpgen.getFileName();
	std::vector<std::string> const fno = SGGF.merge(ffn);

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapConcatDecoder SGGD(fno);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
}

void testsparsegammamultimerge2()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileSet2 SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder2 SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	std::string const ffn = tmpgen.getFileName();
	std::vector<std::string> const fno = SGGF.merge(ffn);

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapConcatDecoder2 SGGD(fno);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
}

#include <libmaus2/gamma/SparseGammaGapMultiFileLevelSet.hpp>

void testsparsegammamultifilesetmerge()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		//libmaus2::aio::CheckedInputOutputStream indexCIOS(indexfn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	std::string const ffn = tmpgen.getFileName();
	uint64_t const numthreads = libmaus2::parallel::NumCpus::getNumLogicalProcessors();
	std::vector<std::string> const fno = SGGF.merge(ffn,numthreads);

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapConcatDecoder SGGD(fno);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
}

void testsparsegammamultifilesetmerge2()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet2 SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		//libmaus2::aio::CheckedInputOutputStream indexCIOS(indexfn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder2 SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	std::string const ffn = tmpgen.getFileName();
	uint64_t const numthreads = libmaus2::parallel::NumCpus::getNumLogicalProcessors();
	std::vector<std::string> const fno = SGGF.merge(ffn,numthreads);

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::SparseGammaGapConcatDecoder2 SGGD(fno);
	for ( uint64_t i = 0; i < 60; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
}

void testsparsegammamultifilesetmergedense()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;
	uint64_t s = 0;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		//libmaus2::aio::CheckedInputOutputStream indexCIOS(indexfn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		s += i+1;
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		s += i+1;
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		s += i+1;
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	uint64_t const maxval = refM.size() ? (refM.rbegin())->first : 0;

	std::string const ffn = tmpgen.getFileName();
	uint64_t const numthreads = libmaus2::parallel::NumCpus::getNumLogicalProcessors();
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet::MergeDenseResult MDR =
		SGGF.mergeToDense(tmpgen,maxval+1,numthreads);
	std::vector<std::string> const fno = MDR.V;
	assert ( MDR.s == s );

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::GammaGapDecoder SGGD(fno,0 /* offset */,0 /* psymoff */, 1 /* numthreads */);
	for ( uint64_t i = 0; i < maxval+1; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
	{
		// std::cerr << fno[i] << std::endl;
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
	}
}

void testsparsegammamultifilesetmergedense2()
{
	libmaus2::util::TempFileNameGenerator tmpgen("tmp",3);
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet2 SGGF(tmpgen,4);
	std::map<uint64_t,uint64_t> refM;
	uint64_t s = 0;

	for ( uint64_t i = 0; i < 25;  ++i )
	{
		std::string const fn = tmpgen.getFileName();
		std::string const indexfn = fn+".idx";
		libmaus2::aio::OutputStreamInstance COS(fn);
		//libmaus2::aio::CheckedInputOutputStream indexCIOS(indexfn);
		libmaus2::aio::InputOutputStream::unique_ptr_type PindexCIOS(libmaus2::aio::InputOutputStreamFactoryContainer::constructUnique(indexfn,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary));
		libmaus2::gamma::SparseGammaGapBlockEncoder2 SGE(COS,*PindexCIOS);

		SGE.encode(2*i,i+1);   refM[2*i]   += (i+1);
		s += i+1;
		SGE.encode(2*i+2,i+1); refM[2*i+2] += (i+1);
		s += i+1;
		SGE.encode(2*i+4,i+1); refM[2*i+4] += (i+1);
		s += i+1;
		SGE.term();

		SGGF.addFile(fn);

		PindexCIOS.reset();
		libmaus2::aio::FileRemoval::removeFile(indexfn);
	}

	uint64_t const maxval = refM.size() ? (refM.rbegin())->first : 0;

	std::string const ffn = tmpgen.getFileName();
	uint64_t const numthreads = libmaus2::parallel::NumCpus::getNumLogicalProcessors();
	libmaus2::gamma::SparseGammaGapMultiFileLevelSet2::MergeDenseResult MDR =
		SGGF.mergeToDense(tmpgen,maxval+1,numthreads);
	std::vector<std::string> const fno = MDR.V;
	assert ( MDR.s == s );

	// libmaus2::aio::InputStreamInstance CIS(ffn);
	libmaus2::gamma::GammaGapDecoder SGGD(fno,0/* offset */, 0 /* psymoff */, 1 /* numthreads */);
	for ( uint64_t i = 0; i < maxval+1; ++i )
	{
		uint64_t dv = SGGD.decode();

		std::cerr << dv;
		if ( refM.find(i) != refM.end() )
		{
			std::cerr << "(" << refM.find(i)->second << ")";
			assert ( refM.find(i)->second == dv );
		}
		else
		{
			std::cerr << "(0)";
			assert ( dv == 0 );
		}
		std::cerr << ";";
	}
	std::cerr << std::endl;

	for ( uint64_t i = 0; i < fno.size(); ++i )
	{
		// std::cerr << fno[i] << std::endl;
		libmaus2::aio::FileRemoval::removeFile(fno[i]);
	}
}


void testgammadifferencecoding()
{
	std::cerr << "testing difference coding...";

	std::string const fn = "mem://gamma";
	typedef uint64_t gamma_type;
	libmaus2::gamma::GammaDifferenceEncoder<gamma_type>::unique_ptr_type GDE(new libmaus2::gamma::GammaDifferenceEncoder<gamma_type>(fn));
	#if 0
	GDE->encode(0);
	GDE->encode(1);
	GDE->encode(5);
	GDE->encode(7);
	#endif
	for ( uint64_t i = 0; i < 64*1024; ++i )
		GDE->encode(3*i+2);

	GDE.reset();
	libmaus2::gamma::GammaDifferenceDecoder<gamma_type>::unique_ptr_type GDD(new libmaus2::gamma::GammaDifferenceDecoder<gamma_type>(fn));
	gamma_type v;
	uint64_t j = 0;
	while ( GDD->decode(v) )
	{
		uint64_t const low = libmaus2::gamma::GammaDifferenceDecoderNumberCast<gamma_type>::numberCast(v);
		assert ( low == 3*(j++)+2 );
	}

	GDD.reset();
	libmaus2::aio::FileRemoval::removeFile(fn);

	std::cerr << "done." << std::endl;
}

void testgammadifferencecoding2()
{
	std::cerr << "testing difference coding...";

	std::string const fn = "mem://gamma";
	typedef libmaus2::math::UnsignedInteger<4> gamma_type;
	libmaus2::gamma::GammaDifferenceEncoder<gamma_type>::unique_ptr_type GDE(new libmaus2::gamma::GammaDifferenceEncoder<gamma_type>(fn));
	for ( uint64_t i = 0; i < 64*1024; ++i )
		GDE->encode(3*i+2);

	GDE.reset();
	libmaus2::gamma::GammaDifferenceDecoder<gamma_type>::unique_ptr_type GDD(new libmaus2::gamma::GammaDifferenceDecoder<gamma_type>(fn));
	gamma_type v;
	uint64_t j = 0;
	while ( GDD->decode(v) )
	{
		uint64_t const high = libmaus2::gamma::GammaDifferenceDecoderNumberCast<gamma_type>::numberCast(v >> 64);
		uint64_t const low = libmaus2::gamma::GammaDifferenceDecoderNumberCast<gamma_type>::numberCast(v);

		assert ( high == 0 );
		assert ( low == 3*(j++)+2 );
	}

	GDD.reset();
	libmaus2::aio::FileRemoval::removeFile(fn);

	std::cerr << "done." << std::endl;
}

void testpartitionsingle()
{
	std::string const fn = "mem://tmp_part";
	libmaus2::random::Random::setup();
	//uint64_t const mod = 64*1024+27;
	uint64_t const mod = 241;

	for ( uint64_t z = 0; z < 16; ++z )
	{
		libmaus2::gamma::GammaPartitionEncoder::unique_ptr_type Genc(new libmaus2::gamma::GammaPartitionEncoder(fn));
		uint64_t const n = 996 + libmaus2::random::Random::rand64() % 128;
		std::cerr << n << std::endl;

		std::vector<uint64_t> L;
		std::vector< libmaus2::gamma::Interval > LV;
		uint64_t low = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const w = (libmaus2::random::Random::rand64() % mod)+1;
			L.push_back(w);
			uint64_t const high = low+w;
			LV.push_back(libmaus2::gamma::Interval(low,high));
			Genc->put(libmaus2::gamma::Interval(low,high));
			low = high;
		}

		Genc.reset();

		libmaus2::gamma::GammaPartitionDecoder::unique_ptr_type Gdec(new libmaus2::gamma::GammaPartitionDecoder(std::vector<std::string>(1,fn),0 /* offset */,1 /* numthreads */));
		libmaus2::gamma::Interval P;
		uint64_t c = 0;
		while ( Gdec->getNext(P) )
		{
			assert ( P == LV[c] );
			c += 1;
		}
		assert ( c == n );

		Gdec.reset();

		for ( uint64_t i = 0; i < n; ++i )
		{
			libmaus2::gamma::GammaPartitionDecoder::unique_ptr_type Tdec(
				new libmaus2::gamma::GammaPartitionDecoder(std::vector<std::string>(1,fn),LV[i].from /* offset */,1 /* numthreads */));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( ok );
			bool vok = (P == LV[i]);
			if ( ! vok )
			{
				libmaus2::gamma::GammaPartitionDecoder::unique_ptr_type Tdec(
					new libmaus2::gamma::GammaPartitionDecoder(std::vector<std::string>(1,fn),LV[i].from /* offset */,1/*numthreads */));

				std::cerr << "expect " << LV[i].from << "," << LV[i].to << " got " << P.from << "," << P.to << std::endl;
				if ( i )
					std::cerr << "prev " << LV[i-1].from << "," << LV[i-1].to << std::endl;
				if ( i+1 < n )
					std::cerr << "next " << LV[i+1].from << "," << LV[i+1].to << std::endl;
				std::cerr << "i=" << i << std::endl;
			}
			assert ( P == LV[i] );

			Gdec.reset();
		}

		{
			libmaus2::gamma::GammaPartitionDecoder::unique_ptr_type Tdec(new libmaus2::gamma::GammaPartitionDecoder(std::vector<std::string>(1,fn),LV.back().to,1 /* numthreads */));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( !ok );

			Gdec.reset();
		}

		for ( uint64_t i = 0; i < n; ++i )
			for ( uint64_t j = LV[i].from; j < LV[i].to; ++j )
			{
				libmaus2::gamma::GammaPartitionDecoder::unique_ptr_type Tdec(new libmaus2::gamma::GammaPartitionDecoder(std::vector<std::string>(1,fn),j,1 /* numthreads */));
				Gdec = UNIQUE_PTR_MOVE(Tdec);

				bool const ok = Gdec->getNext(P);
				assert ( ok );
				assert ( P == LV[i] );

				Gdec.reset();
			}
	}
}

void testintervalsingle()
{
	std::string const fn = "mem://tmp_part";
	libmaus2::random::Random::setup();
	//uint64_t const mod = 64*1024+27;
	uint64_t const mod = 241;

	for ( uint64_t z = 0; z < 16; ++z )
	{
		libmaus2::gamma::GammaIntervalEncoder::unique_ptr_type Genc(new libmaus2::gamma::GammaIntervalEncoder(fn));
		uint64_t const n = 996 + libmaus2::random::Random::rand64() % 128;
		std::cerr << n << std::endl;

		std::vector< std::pair<uint64_t,uint64_t> > LV;
		uint64_t offset = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const o = (libmaus2::random::Random::rand64() % mod)+1;
			uint64_t const w = (libmaus2::random::Random::rand64() % mod)+1;

			uint64_t const low = offset + o;
			uint64_t const high = low + w;

			LV.push_back(std::pair<uint64_t,uint64_t>(low,high));
			Genc->put(std::pair<uint64_t,uint64_t>(low,high));

			offset = high;
		}

		Genc.reset();

		libmaus2::gamma::GammaIntervalDecoder::unique_ptr_type Gdec(new libmaus2::gamma::GammaIntervalDecoder(std::vector<std::string>(1,fn),0 /* offset */,1 /* numthreads */));
		std::pair<uint64_t,uint64_t> P;
		uint64_t c = 0;
		while ( Gdec->getNext(P) )
		{
			assert ( P == LV[c] );
			c += 1;
		}
		assert ( c == n );

		Gdec.reset();

		uint64_t vsum = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			libmaus2::gamma::GammaIntervalDecoder::unique_ptr_type Tdec(
				new libmaus2::gamma::GammaIntervalDecoder(std::vector<std::string>(1,fn),vsum,1/* numthreads */));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( ok );
			assert ( P == LV[i] );

			Gdec.reset();

			vsum += LV[i].second-LV[i].first;
		}

		vsum = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const w = LV[i].second-LV[i].first;

			for ( uint64_t j = 0; j < w; ++j )
			{
				libmaus2::gamma::GammaIntervalDecoder::unique_ptr_type Tdec(
					new libmaus2::gamma::GammaIntervalDecoder(std::vector<std::string>(1,fn),vsum+j,1/* numthreads */));
				Gdec = UNIQUE_PTR_MOVE(Tdec);

				bool const ok = Gdec->getNext(P);
				assert ( ok );
				assert ( P == LV[i] );

				Gdec.reset();
			}

			vsum += LV[i].second-LV[i].first;
		}
	}
}

#include <libmaus2/gamma/GammaFlaggedPartitionEncoder.hpp>
#include <libmaus2/gamma/GammaFlaggedPartitionDecoder.hpp>

void testflaggedpartitionsingle()
{
	std::string const fn = "mem://tmp_part";
	libmaus2::random::Random::setup();
	//uint64_t const mod = 64*1024+27;
	uint64_t const mod = 241;

	for ( uint64_t z = 0; z < 16; ++z )
	{
		libmaus2::gamma::GammaFlaggedPartitionEncoder::unique_ptr_type Genc(new libmaus2::gamma::GammaFlaggedPartitionEncoder(fn));
		uint64_t const n = 996 + libmaus2::random::Random::rand64() % 128;
		std::cerr << n << std::endl;

		std::vector<uint64_t> L;
		std::vector< libmaus2::gamma::FlaggedInterval > LV;
		uint64_t low = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const w = (libmaus2::random::Random::rand64() % mod)+1;
			L.push_back(w);
			uint64_t const high = low+w;
			libmaus2::gamma::FlaggedInterval::interval_type const flag = static_cast<libmaus2::gamma::FlaggedInterval::interval_type>(i % 4); // libmaus2::random::Random::rand8() % 2;
			LV.push_back(libmaus2::gamma::FlaggedInterval(low,high,flag,false /* active */));
			Genc->put(LV.back());
			low = high;
		}

		Genc.reset();

		libmaus2::gamma::GammaFlaggedPartitionDecoder::unique_ptr_type Gdec(new libmaus2::gamma::GammaFlaggedPartitionDecoder(std::vector<std::string>(1,fn),0 /* offset */,1/*numthreads */));
		libmaus2::gamma::FlaggedInterval P;
		uint64_t c = 0;
		while ( Gdec->getNext(P) )
		{
			assert ( P == LV[c] );
			c += 1;
		}
		assert ( c == n );

		Gdec.reset();

		for ( uint64_t i = 0; i < n; ++i )
		{
			libmaus2::gamma::GammaFlaggedPartitionDecoder::unique_ptr_type Tdec(
				new libmaus2::gamma::GammaFlaggedPartitionDecoder(std::vector<std::string>(1,fn),LV[i].from /* offset */,1 /* numthreads */));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( ok );
			bool vok = (P == LV[i]);
			if ( ! vok )
			{
				libmaus2::gamma::GammaFlaggedPartitionDecoder::unique_ptr_type Tdec(
					new libmaus2::gamma::GammaFlaggedPartitionDecoder(std::vector<std::string>(1,fn),LV[i].from /* offset */,1/*numthreads */));

				std::cerr << "expect " << LV[i].from << "," << LV[i].to << " got " << P.from << "," << P.to << std::endl;
				if ( i )
					std::cerr << "prev " << LV[i-1].from << "," << LV[i-1].to << std::endl;
				if ( i+1 < n )
					std::cerr << "next " << LV[i+1].from << "," << LV[i+1].to << std::endl;
				std::cerr << "i=" << i << std::endl;
			}
			assert ( P == LV[i] );

			Gdec.reset();
		}

		{
			libmaus2::gamma::GammaFlaggedPartitionDecoder::unique_ptr_type Tdec(new libmaus2::gamma::GammaFlaggedPartitionDecoder(std::vector<std::string>(1,fn),LV.back().to,1 /* numthreads */));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( !ok );

			Gdec.reset();
		}

		for ( uint64_t i = 0; i < n; ++i )
			for ( uint64_t j = LV[i].from; j < LV[i].to; ++j )
			{
				libmaus2::gamma::GammaFlaggedPartitionDecoder::unique_ptr_type Tdec(new libmaus2::gamma::GammaFlaggedPartitionDecoder(std::vector<std::string>(1,fn),j,1 /* numthreads */));
				Gdec = UNIQUE_PTR_MOVE(Tdec);

				bool const ok = Gdec->getNext(P);
				assert ( ok );
				assert ( P == LV[i] );

				Gdec.reset();
			}
	}
}

#include <libmaus2/gamma/GammaFlaggedIntervalDecoder.hpp>
#include <libmaus2/gamma/GammaFlaggedIntervalEncoder.hpp>

void testflaggedintervalsingle()
{
	std::string const fn = "mem://tmp_part";
	libmaus2::random::Random::setup();
	//uint64_t const mod = 64*1024+27;
	uint64_t const mod = 241;

	for ( uint64_t z = 0; z < 16; ++z )
	{
		libmaus2::gamma::GammaFlaggedIntervalEncoder::unique_ptr_type Genc(new libmaus2::gamma::GammaFlaggedIntervalEncoder(fn));
		uint64_t const n = 996 + libmaus2::random::Random::rand64() % 128;
		std::cerr << n << std::endl;

		std::vector< libmaus2::gamma::FlaggedInterval > LV;
		uint64_t offset = 0;
		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const o = (libmaus2::random::Random::rand64() % mod)+1;
			uint64_t const w = (libmaus2::random::Random::rand64() % mod)+1;

			uint64_t const low = offset + o;
			uint64_t const high = low + w;

			LV.push_back(libmaus2::gamma::FlaggedInterval(low,high,libmaus2::gamma::FlaggedInterval::interval_type_complete,(i%3)==0 /*active*/));
			Genc->put(LV.back());

			offset = high;
		}

		Genc.reset();

		libmaus2::gamma::GammaFlaggedIntervalDecoder::unique_ptr_type Gdec(new libmaus2::gamma::GammaFlaggedIntervalDecoder(std::vector<std::string>(1,fn),0 /* offset */));
		libmaus2::gamma::FlaggedInterval P;
		uint64_t c = 0;
		while ( Gdec->getNext(P) )
		{
			assert ( P == LV[c] );
			c += 1;
		}
		assert ( c == n );

		Gdec.reset();

		for ( uint64_t i = 0; i < n; ++i )
		{
			libmaus2::gamma::GammaFlaggedIntervalDecoder::unique_ptr_type Tdec(
				new libmaus2::gamma::GammaFlaggedIntervalDecoder(std::vector<std::string>(1,fn),LV[i].from));
			Gdec = UNIQUE_PTR_MOVE(Tdec);

			bool const ok = Gdec->getNext(P);
			assert ( ok );
			assert ( P == LV[i] );

			Gdec.reset();
		}

		for ( uint64_t i = 0; i < n; ++i )
		{
			uint64_t const w = LV[i].to-LV[i].from;

			for ( uint64_t j = 0; j < w; ++j )
			{
				libmaus2::gamma::GammaFlaggedIntervalDecoder::unique_ptr_type Tdec(
					new libmaus2::gamma::GammaFlaggedIntervalDecoder(std::vector<std::string>(1,fn),LV[i].from+j));
				Gdec = UNIQUE_PTR_MOVE(Tdec);

				bool const ok = Gdec->getNext(P);
				assert ( ok );
				assert ( P == LV[i] );

				Gdec.reset();
			}
		}

		for ( uint64_t i = 0; i < n; ++i )
		{
			libmaus2::gamma::GammaFlaggedIntervalDecoder::unique_ptr_type Gdec(new libmaus2::gamma::GammaFlaggedIntervalDecoder(std::vector<std::string>(1,fn),i /* offset */,libmaus2::gamma::GammaFlaggedIntervalDecoder::init_mode_interval_id));

			for ( uint64_t j = i; j < n; ++j )
			{
				bool const ok = Gdec->getNext(P);
				assert ( ok );
				assert ( P == LV[j] );
				//std::cerr << "ok" << std::endl;
			}
		}
	}

	std::cerr << "@" << std::endl;
}

void testIt()
{
	std::string const fn = "mem://tmp_part";
        libmaus2::random::Random::setup();

	libmaus2::gamma::GammaFlaggedIntervalEncoder::unique_ptr_type Genc(new libmaus2::gamma::GammaFlaggedIntervalEncoder(fn));
	Genc->put(libmaus2::gamma::FlaggedInterval(0,0,libmaus2::gamma::FlaggedInterval::interval_type_complete,false));
	Genc->put(libmaus2::gamma::FlaggedInterval(0,0,libmaus2::gamma::FlaggedInterval::interval_type_complete,false));
	Genc->put(libmaus2::gamma::FlaggedInterval(0,0,libmaus2::gamma::FlaggedInterval::interval_type_complete,true));
	Genc->put(libmaus2::gamma::FlaggedInterval(1,2,libmaus2::gamma::FlaggedInterval::interval_type_complete,false));
	Genc->put(libmaus2::gamma::FlaggedInterval(2,2,libmaus2::gamma::FlaggedInterval::interval_type_complete,true));
	Genc->put(libmaus2::gamma::FlaggedInterval(2,2,libmaus2::gamma::FlaggedInterval::interval_type_complete,false));
	Genc->put(libmaus2::gamma::FlaggedInterval(2,2,libmaus2::gamma::FlaggedInterval::interval_type_complete,false));
	Genc->flush();
	Genc.reset();

	for ( uint64_t i = 0; i < 3; ++i )
	{
		libmaus2::gamma::GammaFlaggedIntervalDecoder::unique_ptr_type Tdec(
			new libmaus2::gamma::GammaFlaggedIntervalDecoder(std::vector<std::string>(1,fn),i));
		libmaus2::gamma::FlaggedInterval I;

		std::cerr << i << std::endl;
		while ( Tdec->getNext(I) )
			std::cerr << I << std::endl;
	}
}

int main()
{
	try
	{
		testIt();

		srand(time(0));

		testflaggedintervalsingle();
		std::cerr << "[V] flagged interval test done" << std::endl;

		testflaggedpartitionsingle();
		std::cerr << "[V] flagged partition test done" << std::endl;

		testintervalsingle();
		testpartitionsingle();

		std::cerr << "[V] single and partition tested" << std::endl;

		testRandom2(256*1024*1024);
		testRandom(256*1024*1024);

		testLow2();
		testLow();

		testgammarl();

		testgammadifferencecoding2();
		testgammadifferencecoding();

		testsparsegammamultifilesetmergedense2();
		testsparsegammamultifilesetmergedense();

		testsparsegammamultifilesetmerge2();
		testsparsegammamultifilesetmerge();

		testsparsegammamultimerge2();
		testsparsegammamultimerge();

		testSparseGammaGapMergingRandom2();
		testSparseGammaGapMergingRandom();

		testSparseGammaGapMergingSmall2();
		testSparseGammaGapMergingSmall();

		testSparseGammaConcat2();
		testSparseGammaConcat();
		testSparseGammaIndexing2();
		testSparseGammaIndexing();
		testsparsegammalevelmerge2();
		testsparsegammalevelmerge();
		testsparsegammamerge2();
		testsparsegammamerge();
		testgammasparse();
		testgammasparse2();

		testgammagap();
	}
	catch(std::exception const & ex)
	{
		std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}
}
