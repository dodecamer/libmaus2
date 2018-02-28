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
#if ! defined(LIBMAUS2_BAMBAM_BAMHEADER_HPP)
#define LIBMAUS2_BAMBAM_BAMHEADER_HPP

#include <libmaus2/bambam/BamHeaderParserState.hpp>
#include <libmaus2/lz/GzipStream.hpp>
#include <libmaus2/bambam/Chromosome.hpp>
#include <libmaus2/bambam/EncoderBase.hpp>
#include <libmaus2/bambam/DecoderBase.hpp>
#include <libmaus2/bambam/HeaderLine.hpp>
#include <libmaus2/util/stringFunctions.hpp>
#include <libmaus2/util/unordered_map.hpp>
#include <libmaus2/trie/TrieState.hpp>
#include <libmaus2/lz/BgzfInflateStream.hpp>
#include <libmaus2/lz/BgzfInflateParallelStream.hpp>
#include <libmaus2/lz/BgzfInflateDeflateParallelInputStream.hpp>
#include <libmaus2/hashing/ConstantStringHash.hpp>
#include <libmaus2/bambam/ReadGroup.hpp>
#include <libmaus2/fastx/FastAStreamSet.hpp>
#include <libmaus2/util/OutputFileNameTools.hpp>
#include <libmaus2/lz/BufferedGzipStream.hpp>
#include <libmaus2/fastx/RefPathLookup.hpp>

namespace libmaus2
{
	namespace bambam
	{

		/**
		 * BAM file header class
		 **/
		struct BamHeader :
			public ::libmaus2::bambam::EncoderBase,
			public ::libmaus2::bambam::DecoderBase
		{
			public:
			typedef BamHeader this_type;
			typedef libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;
			typedef libmaus2::util::shared_ptr<this_type>::type shared_ptr_type;

			//! header text
			std::string text;

			private:
			//! chromosome (reference sequence meta data) vector
			std::vector< ::libmaus2::bambam::Chromosome > chromosomes;
			//! read groups vector
			std::vector< ::libmaus2::bambam::ReadGroup > RG;
			//! trie for read group names
			::libmaus2::trie::LinearHashTrie<char,uint32_t>::shared_ptr_type RGTrie;
			//! hash for read group names
			libmaus2::hashing::ConstantStringHash::shared_ptr_type RGCSH;
			//! library names
			std::vector<std::string> libs;
			//! number of libaries
			uint64_t numlibs;

			public:
			int64_t getMaximumSequenceSize() const
			{
				int64_t s = 0;
				for ( uint64_t i = 0; i < chromosomes.size(); ++i )
					s = std::max(s,static_cast<int64_t>(chromosomes[i].getLength()));
				return s;
			}

			bool checkSequenceChecksumsCached(bool const dothrow)
			{
				libmaus2::fastx::RefPathLookup RPL;

				for ( size_t z = 0; z < chromosomes.size(); ++z )
				{
					::libmaus2::bambam::Chromosome & chr = chromosomes[z];
					std::vector< std::pair<std::string,std::string> > KV = chr.getSortedKeyValuePairs();

					std::string m5;
					bool havem5 = false;

					for ( size_t i = 0; i < KV.size(); ++i )
						if ( KV[i].first == "M5" )
						{
							havem5 = true;
							m5 = KV[i].second;
						}

					if ( ! havem5 )
					{
						libmaus2::exception::LibMausException lme;
						lme.getStream() << "libmaus2::bambam::BamHeader: no M5 field for sequence " << chr.getNameCString() << std::endl;
						lme.finish();
						throw lme;
					}

					if ( ! RPL.sequenceCached(m5) )
					{
						if ( dothrow )
						{
							libmaus2::exception::LibMausException lme;
							lme.getStream() << "libmaus2::bambam::BamHeader: no cached sequence found for " << chr.getNameCString() << std::endl;
							lme.finish();
							throw lme;
						}
						else
						{
							return false;
						}
					}
				}

				return true;
			}

			std::vector<std::string> getSequenceURSet(bool fillRefCache) const
			{
				std::set<std::string> S;

				for ( size_t z = 0; z < chromosomes.size(); ++z )
				{
					std::vector< std::pair<std::string,std::string> > KV = chromosomes[z].getSortedKeyValuePairs();

					for ( size_t i = 0; i < KV.size(); ++i )
						if ( KV[i].first == "UR" )
							S.insert(KV[i].second);
				}

				if ( fillRefCache )
				{
					for ( std::set < std::string >::const_iterator ita = S.begin(); ita != S.end(); ++ita )
					{
						libmaus2::aio::InputStream::unique_ptr_type Pin(libmaus2::aio::InputStreamFactoryContainer::constructUnique(*ita));
						std::istream * pin = Pin.get();
						libmaus2::lz::BufferedGzipStream::unique_ptr_type Pdecomp;

						if ( libmaus2::util::OutputFileNameTools::endsOn(*ita,".gz") )
						{
							libmaus2::lz::BufferedGzipStream::unique_ptr_type Tdecomp(new libmaus2::lz::BufferedGzipStream(*pin));
							Pdecomp = UNIQUE_PTR_MOVE(Tdecomp);
							pin = Pdecomp.get();
						}

						libmaus2::fastx::FastAStreamSet FASS(*pin);
						FASS.computeMD5(true /* write */,false /* verify cache */);
					}
				}

				return std::vector<std::string>(S.begin(),S.end());
			}

			/**
			 * check the SQ lines for missing M5 fields and insert the fields if possible by scanning
			 * a reference FastA file. The reference FastA file is decompressed on the fly if its file name
			 * ends on .gz . References can be loaded from local files, ftp or http.
			 *
			 * @param reference name of reference file to be used for lines without both M5 and UR fields
			 **/
			void checkSequenceChecksums(std::string reference = std::string())
			{
				if ( reference.size() && libmaus2::aio::InputStreamFactoryContainer::tryOpen(reference) && reference[0] != '/' )
				{
					libmaus2::autoarray::AutoArray<char> cwdspace(std::max(static_cast<uint64_t>(2*PATH_MAX),static_cast<uint64_t>(1)));
					char * p = NULL;
					while ( (! (p=getcwd(cwdspace.begin(),cwdspace.size()))) && errno == ERANGE )
						cwdspace.resize(2*cwdspace.size());
					if ( ! p )
					{
						libmaus2::exception::LibMausException lme;
						lme.getStream() << "libmaus2::bambam::BamHeader: failed to get current directory." << std::endl;
						lme.finish();
						throw lme;
					}
					reference = std::string(cwdspace.begin()) + "/" + reference;
				}

				bool haveallm5 = true;
				std::set < std::string > nom5ur;

				for ( size_t z = 0; z < chromosomes.size(); ++z )
				{
					::libmaus2::bambam::Chromosome & chr = chromosomes[z];
					std::vector< std::pair<std::string,std::string> > KV = chr.getSortedKeyValuePairs();

					std::string m5, ur;
					bool havem5 = false, haveur = false;

					for ( size_t i = 0; i < KV.size(); ++i )
						if ( KV[i].first == "M5" )
						{
							havem5 = true;
							m5 = KV[i].second;
						}
						else if ( KV[i].first == "UR" )
						{
							haveur = true;
							ur = KV[i].second;
						}

					haveallm5 = haveallm5 && havem5;

					if ( ! havem5 && ! haveur )
					{
						if ( reference.size() )
						{
							ur = "file://" + reference;
							haveur = true;

							if ( chr.getRestKVString().size() )
								chr.setRestKVString( chr.getRestKVString() + "\tUR:" + ur );
							else
								chr.setRestKVString( std::string("UR:") + ur );
						}
						else
						{
							libmaus2::exception::LibMausException lme;
							lme.getStream() << "libmaus2::bambam::BamHeader: sequence " << chr.getNameCString() << " has neither M5 nor UR field" << std::endl;
							lme.finish();
							throw lme;
						}
					}

					if ( ! havem5 )
					{
						nom5ur.insert(ur);
					}
				}

				std::map < std::string, std::map<std::string,std::string> > M;

				for ( std::set < std::string >::const_iterator ita = nom5ur.begin(); ita != nom5ur.end(); ++ita )
				{
					libmaus2::aio::InputStream::unique_ptr_type Pin(libmaus2::aio::InputStreamFactoryContainer::constructUnique(*ita));
					std::istream * pin = Pin.get();
					libmaus2::lz::BufferedGzipStream::unique_ptr_type Pdecomp;

					if ( libmaus2::util::OutputFileNameTools::endsOn(*ita,".gz") )
					{
						libmaus2::lz::BufferedGzipStream::unique_ptr_type Tdecomp(new libmaus2::lz::BufferedGzipStream(*pin));
						Pdecomp = UNIQUE_PTR_MOVE(Tdecomp);
						pin = Pdecomp.get();
					}

					libmaus2::fastx::FastAStreamSet FASS(*pin);
					// id -> digest
					std::map<std::string,std::string> submap = FASS.computeMD5(true /* write */,false /* verify cache */);
					M [ *ita ] = submap;
				}

				for ( size_t z = 0; z < chromosomes.size(); ++z )
				{
					::libmaus2::bambam::Chromosome & chr = chromosomes[z];
					std::vector< std::pair<std::string,std::string> > KV = chr.getSortedKeyValuePairs();

					std::string m5, ur;
					bool havem5 = false;
					#if ! defined(NDEBUG)
					bool haveur = false;
					#endif

					for ( size_t i = 0; i < KV.size(); ++i )
						if ( KV[i].first == "M5" )
						{
							havem5 = true;
							m5 = KV[i].second;
						}
						else if ( KV[i].first == "UR" )
						{
							#if ! defined(NDEBUG)
							haveur = true;
							#endif
							ur = KV[i].second;
						}

					if ( ! havem5 )
					{
						#if ! defined(NDEBUG)
						assert ( haveur );
						#endif

						if ( M.find(ur) == M.end() )
						{
							libmaus2::exception::LibMausException lme;
							lme.getStream() << "libmaus2::bambam::BamHeader: failed to get data for URL " << ur << std::endl;
							lme.finish();
							throw lme;
						}

						std::map<std::string,std::string> const & submap = M.find(ur)->second;

						if ( submap.find(chr.getNameString()) == submap.end() )
						{
							libmaus2::exception::LibMausException lme;
							lme.getStream() << "libmaus2::bambam::BamHeader: sequence " << chr.getNameString() << " not found in file " << ur << std::endl;
							lme.finish();
							throw lme;
						}

						std::string const sdigest = submap.find(chr.getNameString())->second;

						chr.setRestKVString(chr.getRestKVString() + "\tM5:" + sdigest);
					}
				}

				std::istringstream istr(text);
				std::ostringstream ostr;

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() )
					{
						if ( !(startsWith(line,"@SQ")) )
							ostr << line << std::endl;
					}
				}

				for ( size_t i = 0; i < chromosomes.size(); ++i )
					ostr << chromosomes[i].createLine() << std::endl;

				text = ostr.str();
			}

			/**
			 * clone this object and return clone in a unique pointer
			 *
			 * @return clone of this object
			 **/
			unique_ptr_type uclone() const
			{
				unique_ptr_type O(new this_type);
				O->text = this->text;
				O->chromosomes = this->chromosomes;
				O->RG = this->RG;
				if ( this->RGTrie.get() )
					O->RGTrie = this->RGTrie->sclone();
				if ( this->RGCSH.get() )
					O->RGCSH = this->RGCSH->sclone();
				O->libs = this->libs;
				O->numlibs = this->numlibs;
				return UNIQUE_PTR_MOVE(O);
			}

			/**
			 * clone this object and return clone in a shared pointer
			 *
			 * @return clone of this object
			 **/
			shared_ptr_type sclone() const
			{
				shared_ptr_type O(new this_type);
				O->text = this->text;
				O->chromosomes = this->chromosomes;
				O->RG = this->RG;
				if ( this->RGTrie.get() )
					O->RGTrie = this->RGTrie->sclone();
				if ( this->RGCSH.get() )
					O->RGCSH = this->RGCSH->sclone();
				O->libs = this->libs;
				O->numlibs = this->numlibs;
				return O;
			}

			/**
			 * get name for reference id
			 *
			 * @param refid reference id
			 * @return name for reference id or "*" if invalid ref id
			 **/
			char const * getRefIDName(int64_t const refid) const
			{
				if ( refid < 0 || refid >= static_cast<int64_t>(chromosomes.size()) )
					return "*";
				else
				{
					return chromosomes[refid].getNameCString();
				}
			}

			/**
			 * get reference id length
			 **/
			int64_t getRefIDLength(int64_t const refid) const
			{
				if ( refid < 0 || refid >= static_cast<int64_t>(chromosomes.size()) )
					return -1;
				else
					return chromosomes[refid].getLength();
			}

			/**
			 * get number of reference sequences
			 **/
			uint64_t getNumRef() const
			{
				return chromosomes.size();
			}

			/**
			 * get vector of read groups
			 *
			 * @return read group vector
			 **/
			std::vector<ReadGroup> const & getReadGroups() const
			{
				return RG;
			}

			/**
			 * get vector of chromosomes
			 *
			 * @return chromosome vector
			 **/
			std::vector<Chromosome> const & getChromosomes() const
			{
				return chromosomes;
			}

			/**
			 * get string identifier fur numeric read group i
			 *
			 * @param i numeric identifier for read group
			 * @return string identifier for read group i in the header
			 **/
			std::string const & getReadGroupIdentifierAsString(int64_t const i) const
			{
				if ( i < 0 || i >= static_cast<int64_t>(getNumReadGroups()) )
				{
					libmaus2::exception::LibMausException se;
					se.getStream() << "BamHeader::getReadGroupIdentifierAsString(): invalid numeric id " << i << std::endl;
					se.finish();
					throw se;
				}
				return RG[i].ID;
			}

			/**
			 * get number of read groups
			 *
			 * @return number of read groups
			 **/
			uint64_t getNumReadGroups() const
			{
				return RG.size();
			}

			/**
			 * get read group numerical id for read group name
			 *
			 * @param ID read group name
			 * @return read group numerical id
			 **/
			int64_t getReadGroupId(char const * ID) const
			{
				if ( ID )
				{
					unsigned int const idlen = strlen(ID);

					if ( RGCSH )
					{
						return (*RGCSH)[ ReadGroup::hash(ID,ID+idlen) ];
					}
					else
					{
						return RGTrie->searchCompleteNoFailure(ID,ID+idlen);
					}
				}
				else
					return -1;
			}

			/**
			 * get read group object for read group name
			 *
			 * @param ID read group name
			 * @return read group object for ID
			 **/
			::libmaus2::bambam::ReadGroup const * getReadGroup(char const * ID) const
			{
				int64_t const id = ID ? getReadGroupId(ID) : -1;

				if ( id < 0 )
					return 0;
				else
					return &(RG[id]);
			}

			/**
			 * get library name for library id
			 *
			 * @param libid library id
			 * @return name of library for libid of "Unknown Library" if libid is invalid
			 **/
			std::string getLibraryName(int64_t const libid) const
			{
				if ( libid >= static_cast<int64_t>(numlibs) )
					return "Unknown Library";
				else
					return libs[libid];
			}

			/**
			 * get library name for read group id
			 *
			 * @param ID read group id
			 * @return library name for ID
			 **/
			std::string getLibraryName(char const * ID) const
			{
				return getLibraryName(getLibraryId(ID));
			}

			/**
			 * get library id for read group id
			 *
			 * @param ID read group id string
			 * @return library id for ID
			 **/
			int64_t getLibraryId(char const * ID) const
			{
				int64_t const rgid = getReadGroupId(ID);
				if ( rgid < 0 )
					return numlibs;
				else
					return RG[rgid].LBid;
			}

			/**
			 * get library id for numerical read group id
			 *
			 * @param rgid numerical read group id
			 * @return library id
			 **/
			int64_t getLibraryId(int64_t const rgid) const
			{
				if ( rgid < 0 )
					return numlibs;
				else
					return RG[rgid].LBid;
			}

			/**
			 * compute trie for read group names
			 *
			 * @param RG read group vector
			 * @return trie for read group names
			 **/
			static ::libmaus2::trie::LinearHashTrie<char,uint32_t>::shared_ptr_type computeRgTrie(std::vector< ::libmaus2::bambam::ReadGroup > const & RG)
			{
				::libmaus2::trie::Trie<char> trienofailure;
				std::vector<std::string> dict;
				for ( uint64_t i = 0; i < RG.size(); ++i )
					dict.push_back(RG[i].ID);
				trienofailure.insertContainer(dict);
				::libmaus2::trie::LinearHashTrie<char,uint32_t>::unique_ptr_type LHTnofailure
					(trienofailure.toLinearHashTrie<uint32_t>());
				::libmaus2::trie::LinearHashTrie<char,uint32_t>::shared_ptr_type LHTsnofailure(
					LHTnofailure.release()
					);

				return LHTsnofailure;
			}

			/**
			 * @param s string
			 * @param prefix other string
			 * @return true if s starts with prefix
			 **/
			static bool startsWith(std::string const & s, std::string const & prefix)
			{
				return
					s.size() >= prefix.size()
					&&
					s.substr(0,prefix.size()) == prefix;
			}

			/**
			 * extract read group vector from header text
			 *
			 * @param header text header
			 * @return read groups vector
			 **/
			static std::vector<ReadGroup> getReadGroups(std::string const & header)
			{
				std::vector<ReadGroup> RG;
				std::istringstream istr(header);

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() )
					{
						if (
							(startsWith(line,"@RG"))
						)
						{
							std::deque<std::string> tokens = ::libmaus2::util::stringFunctions::tokenize<std::string>(line,"\t");
							ReadGroup RGI;
							for ( uint64_t i = 1; i < tokens.size(); ++i )
								if ( startsWith(tokens[i],"ID:") )
								{
									RGI.ID = tokens[i].substr(3);
								}
								else if ( tokens[i].size() < 3 || tokens[i][2] != ':' )
								{
									continue;
								}
								else
								{
									std::string const tag = tokens[i].substr(0,2);
									std::string const val = tokens[i].substr(3);
									RGI.M[tag] = val;
								}

							if ( RGI.ID.size() )
								RG.push_back(RGI);

							// std::cerr << RGI << std::endl;
						}
					}
				}

				return RG;
			}

			/**
			 * filter header by removing HD and SQ lines (keep rest)
			 *
			 * @param header text header
			 * @return filtered header
			 **/
			static std::string filterHeader(std::string const & header)
			{
				std::istringstream istr(header);
				std::ostringstream ostr;

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() )
					{
						if (
							!(startsWith(line,"@HD"))
							&&
							!(startsWith(line,"@SQ"))
						)
							ostr << line << std::endl;
					}
				}

				return ostr.str();
			}

			/**
			 * filter out checksum lines
			 *
			 * @param header text header
			 * @return filtered header
			 **/
			static std::string filterOutChecksum(std::string const & header)
			{
				std::istringstream istr(header);
				std::ostringstream ostr;

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() && (!startsWith(line,"@CO\tTY:checksum")) )
						ostr << line << std::endl;
				}

				return ostr.str();
			}

			/**
			 * get version from header; if no HD line is present or it contains no version number, then
			 * return defaultVersion
			 *
			 * @param header text header
			 * @param defaultVersion is "1.4"
			 * @return BAM file version
			 **/
			static std::string getVersionStatic(std::string const & header, std::string const defaultVersion = "1.4")
			{
				std::istringstream istr(header);
				std::string version = defaultVersion;

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() )
					{
						if (
							(startsWith(line,"@HD"))
						)
						{
							std::deque<std::string> tokens = ::libmaus2::util::stringFunctions::tokenize<std::string>(line,"\t");
							for ( uint64_t i = 0; i < tokens.size(); ++i )
								if ( startsWith(tokens[i],"VN:") )
									version = tokens[i].substr(3);
						}
					}
				}

				return version;
			}

			/**
			 * get version BAM file version from header text
			 *
			 * @param defaultVersion is returned if no version is present in the text
			 * @return BAM file version
			 **/
			std::string getVersion(std::string const defaultVersion = "1.4")
			{
				return getVersionStatic(text,defaultVersion);
			}

			/**
			 * get sort order from header; if sort order is not present in text then return defaultSortOrder
			 *
			 * @param header text header
			 * @param defaultSortOrder order to be assume if no order is given in header
			 * @return BAM sort order as recorded in header or given default
			 **/
			static std::string getSortOrderStatic(std::string const & header, std::string const defaultSortOrder = "unknown")
			{
				std::istringstream istr(header);
				std::string sortorder = defaultSortOrder;

				while ( istr )
				{
					std::string line;
					::std::getline(istr,line);
					if ( istr && line.size() )
					{
						if (
							(startsWith(line,"@HD"))
						)
						{
							std::deque<std::string> tokens = ::libmaus2::util::stringFunctions::tokenize<std::string>(line,"\t");
							for ( uint64_t i = 0; i < tokens.size(); ++i )
								if ( startsWith(tokens[i],"SO:") )
									sortorder = tokens[i].substr(3);
						}
					}
				}

				return sortorder;
			}

			/**
			 * @param defaultSortorder default order to be returned if no order is recorded in the text
			 * @return BAM sort order
			 **/
			std::string getSortOrder(std::string const defaultSortorder = "unknown")
			{
				return getSortOrderStatic(text,defaultSortorder);
			}

			/**
			 * rewrite BAM header text
			 *
			 * @param header input header
			 * @param chromosomes reference sequence information to be inserted in rewritten header
			 * @param rsortorder sort order override if not the empty string
			 * @return rewritten header text
			 **/
			static std::string rewriteHeader(
				std::string const & header, std::vector< ::libmaus2::bambam::Chromosome > const & chromosomes,
				std::string const & rsortorder = std::string()
			)
			{
				// get header lines vector
				std::vector<HeaderLine> const hlv = ::libmaus2::bambam::HeaderLine::extractLines(header);
				// pointer to line with HD identifier
				HeaderLine const * hdline = 0;
				// SQ map
				std::map<std::string,HeaderLine const *> sqmap;
				for ( uint64_t i = 0; i < hlv.size(); ++i )
					if ( hlv[i].type == "HD" )
						hdline = &(hlv[i]);
					else if ( hlv[i].type == "SQ" )
						sqmap[ hlv[i].getValue("SN") ] = &(hlv[i]);

				std::ostringstream ostr;

				if ( hdline )
				{
					// if we want to set a different sort order
					if ( rsortorder.size() )
					{
						// tokenize the HD line
						std::deque<std::string> tokens = ::libmaus2::util::stringFunctions::tokenize(hdline->line,std::string("\t"));

						// check the tokens and search for the SO field
						bool foundSO = false;
						for ( uint64_t i = 1; i < tokens.size(); ++i )
							if ( tokens[i].size() >= 3 && tokens[i].substr(0,3) == "SO:" )
							{
								tokens[i] = "SO:" + rsortorder;
								foundSO = true;
							}
						// if there was no sort order previously then add one
						if ( ! foundSO )
							tokens.push_back(std::string("SO:") + rsortorder);

						std::ostringstream hdlinestr;
						for ( uint64_t i = 0; i < tokens.size(); ++i )
						{
							hdlinestr << tokens[i];
							if ( i+1 < tokens.size() )
								hdlinestr << "\t";
						}

						//hdline->line = hdlinestr.str();

						ostr << hdlinestr.str() << std::endl;
					}
					else
					{
						ostr << hdline->line << std::endl;
					}
				}
				else
					ostr << "@HD"
						<< "\tVN:" << getVersionStatic(header)
						<< "\tSO:" << (rsortorder.size() ? rsortorder : getSortOrderStatic(header))
						<< "\n";

				for ( uint64_t i = 0; i < chromosomes.size(); ++i )
				{
					std::pair<char const *,char const *> chrP = chromosomes[i].getName();
					std::string const chrname(chrP.first,chrP.second);

					if ( sqmap.find(chrname) != sqmap.end() )
						ostr << sqmap.find(chrname)->second->line << std::endl;
					else
						ostr << "@SQ\tSN:" << chrname << "\tLN:" << chromosomes[i].getLength() << "\n";
				}

				ostr << filterHeader(header);

				return ostr.str();
			}

			/**
			 * encode binary reference sequence information
			 *
			 * @param ostr binary BAM header construction stream
			 * @param V chromosomes (ref seqs)
			 **/
			template<typename stream_type>
			static void encodeChromosomeVector(stream_type & ostr, std::vector< ::libmaus2::bambam::Chromosome > const & V)
			{
				::libmaus2::bambam::EncoderBase::putLE<stream_type,int32_t>(ostr,V.size());

				for ( uint64_t i = 0; i < V.size(); ++i )
				{
					::libmaus2::bambam::Chromosome const & chr = V[i];
					std::pair<char const *, char const *> P = chr.getName();

					::libmaus2::bambam::EncoderBase::putLE<stream_type,int32_t>(ostr,(P.second-P.first)+1);
					ostr.write(P.first,P.second-P.first);
					ostr.put(0);

					::libmaus2::bambam::EncoderBase::putLE<stream_type,int32_t>(ostr,chr.getLength());
				}
			}

			/**
			 * encode chromosome (ref seq info) vector to binary
			 *
			 * @param V reference sequence info vector
			 * @return BAM binary encoding of V
			 **/
			static std::string encodeChromosomeVector(std::vector< ::libmaus2::bambam::Chromosome > const & V)
			{
				std::ostringstream ostr;
				encodeChromosomeVector(ostr,V);
				return ostr.str();
			}

			/**
			 * serialise header to BAM
			 *
			 * @param ostr output stream
			 **/
			template<typename stream_type>
			void serialise(stream_type & ostr) const
			{
				// magic
				ostr.put('B');
				ostr.put('A');
				ostr.put('M');
				ostr.put('\1');

				// length of plain text
				::libmaus2::bambam::EncoderBase::putLE<stream_type,int32_t>(ostr,text.size()/*+1 */);
				// plain text
				ostr.write(text.c_str(),text.size());
				// ostr.put(0);

				encodeChromosomeVector(ostr,chromosomes);
			}

			static void parseHeader(std::istream & in)
			{
				libmaus2::lz::BgzfInflateStream bgzfin(in);
				BamHeaderParserState state;

				while ( ! state.parseHeader(bgzfin,1).first )
				{

				}
			}

			BamHeader(BamHeaderParserState & state)
			{
				init(state);
			}

			void init(BamHeaderParserState & state)
			{
				text = std::string(state.text.begin(),state.text.begin()+state.l_text);
				chromosomes.swap(state.chromosomes);
				initSetup();
			}

			struct HeaderLineSQNameComparator
			{
				bool operator()(HeaderLine const & A, HeaderLine const & B)
				{
					return A.getValue("SN") < B.getValue("SN");
				}
			};

			void initSetup()
			{
				text = rewriteHeader(text,chromosomes);

				RG = getReadGroups(text);
				RGTrie = computeRgTrie(RG);
				RGCSH = libmaus2::hashing::ConstantStringHash::constructShared(RG.begin(),RG.end());

				if ( !RGCSH )
				{
					std::set<std::string> RGids;
					for ( uint64_t i = 0; i < RG.size(); ++i )
						RGids.insert(RG[i].ID);
					if ( RGids.size() != RG.size() )
					{
						libmaus2::exception::LibMausException se;
						se.getStream() << "BamHeader::init(): Read group identifiers are not unique." << std::endl;
						se.finish();
						throw se;
					}
				}

				// extract all library names
				std::set < std::string > slibs;
				for ( uint64_t i = 0; i < RG.size(); ++i )
					if ( RG[i].M.find("LB") != RG[i].M.end() )
						slibs.insert(RG[i].M.find("LB")->second);

				// assign library ids to read groups (if present)
				libs = std::vector<std::string>(slibs.begin(),slibs.end());
				numlibs = libs.size();
				for ( uint64_t i = 0; i < RG.size(); ++i )
					if ( RG[i].M.find("LB") != RG[i].M.end() )
						RG[i].LBid = std::lower_bound(libs.begin(),libs.end(),RG[i].M.find("LB")->second) - libs.begin();
					else
						RG[i].LBid = numlibs;

				std::vector<HeaderLine> headerlines = libmaus2::bambam::HeaderLine::extractLinesByType(text,"SQ");
				std::sort(headerlines.begin(),headerlines.end(),HeaderLineSQNameComparator());

				// fill information from text into refseq info
				for ( uint64_t i = 0; i < chromosomes.size(); ++i )
				{
					typedef std::vector<HeaderLine>::const_iterator it;
					HeaderLine ref;

					ref.type = "SQ";
					ref.M["SN"] = chromosomes[i].getNameString();

					// look for chromosome/refseq in parsed text
					std::pair<it,it> const p = std::equal_range(
						headerlines.begin(),headerlines.end(),
						ref,
						HeaderLineSQNameComparator()
					);

					// if line is in text
					if ( p.first != p.second )
					{
						// get line
						HeaderLine const & line = *(p.first);
						// build string from rest of arguments
						std::ostringstream restkvostr;
						//
						uint64_t restkvostrcont = 0;

						// iterate over key:value pairs
						for ( std::map<std::string,std::string>::const_iterator ita = line.M.begin();
							ita != line.M.end(); ++ita )
						{
							std::pair<std::string,std::string> const pp = *ita;

							// sequence name should fit (or the equal_range call above is broken)
							if ( pp.first == "SN" )
							{
								assert ( pp.second == chromosomes[i].getNameString() );
							}
							// check that sequence length is consistent between text and binary
							else if ( pp.first == "LN" )
							{
								std::istringstream istr(pp.second);
								uint64_t len;
								istr >> len;
								if ( chromosomes[i].getLength() != len )
								{
									libmaus2::exception::LibMausException se;
									se.getStream() << "BAM header is not consistent (binary and text do not match) for " << line.line << std::endl;
									se.finish();
									throw se;
								}
							}
							else
							{
								if ( restkvostrcont++ )
									restkvostr.put('\t');

								restkvostr << pp.first << ":" << pp.second;

								// chromosomes[i].addKeyValuePair(pp.first,pp.second);
							}
						}

						if ( restkvostrcont )
							chromosomes[i].setRestKVString(restkvostr.str());
					}
				}
			}

			/**
			 * init BAM header from uncompressed stream
			 *
			 * @param in input stream
			 **/
			template<typename stream_type>
			void init(stream_type & in)
			{
				uint8_t fmagic[4];

				for ( unsigned int i = 0; i < sizeof(fmagic)/sizeof(fmagic[0]); ++i )
					fmagic[i] = getByte(in);

				if (
					fmagic[0] != 'B' ||
					fmagic[1] != 'A' ||
					fmagic[2] != 'M' ||
					fmagic[3] != '\1' )
				{
					::libmaus2::exception::LibMausException se;
					se.getStream() << "Wrong magic in BamHeader::init()" << std::endl;
					se.finish();
					throw se;
				}

				uint64_t const l_text = getLEInteger(in,4);
				text.resize(l_text);

				for ( uint64_t i = 0; i < text.size(); ++i )
					text[i] = getByte(in);

				uint64_t textlen = 0;
				while ( textlen < text.size() && text[textlen] )
					textlen++;
				text.resize(textlen);

				uint64_t const n_ref = getLEInteger(in,4);

				for ( uint64_t i = 0; i < n_ref; ++i )
				{
					uint64_t l_name = getLEInteger(in,4);
					assert ( l_name );
					std::string name;
					name.resize(l_name-1);
					for ( uint64_t j = 0 ; j < name.size(); ++j )
						name[j] = getByte(in);
					#if ! defined(NDEBUG)
					int c =
					#endif
						getByte(in);
					#if ! defined(NDEBUG)
					assert ( c == 0 );
					#endif
					uint64_t l_ref = getLEInteger(in,4);
					chromosomes.push_back( ::libmaus2::bambam::Chromosome(name,l_ref) );
				}

				initSetup();
			}

			/**
			 * change sort order to newsortorder; if newsortorder is the empty string then
			 * just rewrite header keeping the previous sort order description
			 *
			 * @param newsortorder
			 **/
			void changeSortOrder(std::string const newsortorder = std::string())
			{
				text = rewriteHeader(text,chromosomes,newsortorder);
			}

			/**
			 * produce header text
			 **/
			void produceHeader()
			{
				changeSortOrder();
			}

			/**
			 * get size of BAM header given the name of the BAM file
			 *
			 * @param fn BAM file name
			 * @return length of BAM header
			 **/
			static uint64_t getHeaderSize(std::string const & fn)
			{
				libmaus2::aio::InputStream::unique_ptr_type CIS(libmaus2::aio::InputStreamFactoryContainer::constructUnique(fn));
				::libmaus2::lz::GzipStream GS(*CIS);
				BamHeader header(GS);
				return GS.tellg();
			}

			/**
			 * constructor for empty header
			 **/
			BamHeader()
			{

			}

			/**
			 * constructor from compressed stream in
			 *
			 * @param in compressed stream
			 **/
			BamHeader(std::istream & in)
			{
				::libmaus2::lz::GzipStream GS(in);
				init(GS);
			}

			/**
			 * constructor from compressed general GZ type stream
			 *
			 * @param in gzip intput stream
			 **/
			BamHeader(::libmaus2::lz::GzipStream & in)
			{
				init(in);
			}
			/**
			 * constructor from serial bgzf decompressor
			 *
			 * @param in serial bgzf decompressor
			 **/
			BamHeader(libmaus2::lz::BgzfInflateStream & in)
			{
				init(in);
			}
			/**
			 * constructor from parallel bgzf decompressor
			 *
			 * @param in parallel bgzf decompressor
			 **/
			BamHeader(libmaus2::lz::BgzfInflateParallelStream & in)
			{
				init(in);
			}
			/**
			 * constructor from parallel bgzf decompressor
			 *
			 * @param in parallel bgzf decompressor
			 **/
			BamHeader(libmaus2::lz::BgzfInflateDeflateParallelInputStream & in)
			{
				init(in);
			}

			void replaceReadGroupNames(std::map<std::string,std::string> const & M)
			{
				std::istringstream istr(text);
				std::ostringstream ostr;

				while ( istr )
				{
					std::string line;
					std::getline(istr,line);

					if ( line.size() >= 3 && line[0] == '@' && line[1] == 'R' && line[2] == 'G' )
					{
						HeaderLine HL(line);
						assert ( HL.type == "RG" );

						if ( HL.hasKey("ID") )
						{
							std::string const oldID = HL.getValue("ID");
							std::map<std::string,std::string>::const_iterator it = M.find(oldID);
							if ( it != M.end() )
							{
								HL.M["ID"] = it->second;
								HL.constructLine();
								line = HL.line;
							}
						}
					}

					if ( line.size() )
						ostr << line << '\n';
				}

				*this = BamHeader(ostr.str());
			}

			/**
			 * constructor from header text
			 *
			 * @param text header text
			 **/
			BamHeader(std::string const & text)
			{
				std::ostringstream ostr;

				ostr.put('B');
				ostr.put('A');
				ostr.put('M');
				ostr.put('\1');

				EncoderBase::putLE<std::ostringstream,uint32_t>(ostr,text.size());
				ostr << text;

				std::vector<HeaderLine> hlv = HeaderLine::extractLines(text);
				uint32_t nref = 0;
				for ( uint64_t i = 0; i < hlv.size(); ++i )
					if ( hlv[i].type == "SQ" )
						nref++;

				EncoderBase::putLE<std::ostringstream,uint32_t>(ostr,nref);

				for ( uint64_t i = 0; i < hlv.size(); ++i )
					if ( hlv[i].type == "SQ" )
					{
						std::string const name = hlv[i].getValue("SN");

						EncoderBase::putLE<std::ostringstream,uint32_t>(ostr,name.size()+1);
						ostr << name;
						ostr.put(0);

						std::string const ssn = hlv[i].getValue("LN");
						std::istringstream ssnistr(ssn);
						uint64_t sn;
						ssnistr >> sn;
						EncoderBase::putLE<std::ostringstream,uint32_t>(ostr,sn);
					}

				std::istringstream istr(ostr.str());
				init(istr);
			}

			/**
			 * add a reference sequence to the header
			 *
			 * @param name ref seq name
			 * @param len ref seq length
			 * @return id of the newly inserted ref seq in this header
			 **/
			uint64_t addChromosome(std::string const & name, uint64_t const len)
			{
				uint64_t const id = chromosomes.size();
				chromosomes.push_back(Chromosome(name,len));
				return id;
			}

			/**
			 * get id for reference name
			 *
			 * @param name reference name
			 * @return id for name
			 **/
			uint64_t getIdForRefName(std::string const & name) const
			{
				for ( uint64_t i = 0; i < chromosomes.size(); ++i )
				{
					std::pair<char const *, char const *> const P = chromosomes[i].getName();
					char const * qa = P.first;
					char const * qe = P.second;
					char const * ca = name.c_str();
					char const * ce = ca + name.size();

					// same length
					if ( qe-qa == ce-ca )
					{
						// check for equal sequence of letters
						for ( ; qa != qe ; ++qa, ++ca )
							if ( *qa != *ca )
								break;

						if ( qa == qe )
						{
							assert ( name == chromosomes[i].getNameString() );
							return i;
						}
					}
				}

				libmaus2::exception::LibMausException se;
				se.getStream() << "Reference name " << name << " does not exist in file." << std::endl;
				se.finish();
				throw se;
			}
		};
	}
}
#endif
