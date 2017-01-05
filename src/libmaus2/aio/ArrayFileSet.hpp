/*
    libmaus2
    Copyright (C) 2016 German Tischler

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
#if ! defined(LIBMAUS2_AIO_ARRAYFILESET_HPP)
#define LIBMAUS2_AIO_ARRAYFILESET_HPP

#include <libmaus2/aio/ArrayInputStream.hpp>
#include <libmaus2/aio/ArrayFileContainer.hpp>
#include <libmaus2/aio/ArrayInputStreamFactory.hpp>
#include <libmaus2/aio/InputStreamFactoryContainer.hpp>

namespace libmaus2
{
	namespace aio
	{
		template<typename _iterator>
		struct ArrayFileSet
		{
			typedef _iterator iterator;
			typedef ArrayFileSet<iterator> this_type;
			typedef typename libmaus2::util::unique_ptr<this_type>::type unique_ptr_type;

			private:
			// protocol
			std::string const prot;
			// container
			libmaus2::aio::ArrayFileContainer<iterator> container;
			// url
			std::vector<std::string> Vurl;

			static std::string pointerToString(void * vp)
			{
				std::ostringstream protstr;
				protstr << vp;
				std::string prot = protstr.str();
				assert ( prot.size() >= 2 && prot.substr(0,2) == "0x" );
				prot = prot.substr(2);
				for ( uint64_t i = 0; i < prot.size(); ++i )
					if ( ::std::isalpha(prot[i]) )
					{
						prot[i] = ::std::tolower(prot[i]);
						assert ( prot[i] >= 'a' );
						assert ( prot[i] <= 'f' );
						char const dig = prot[i] - 'a' + 10;
						prot[i] = dig + 'a';
					}
					else
					{
						assert ( ::std::isdigit(prot[i]) );
						char const dig = prot[i] - '0';
						prot[i] = dig + 'a';
					}
				return prot;
			}

			public:
			ArrayFileSet(
				std::vector< std::pair<iterator,iterator> > const & data,
				std::vector<std::string> const & filenames
			)
			: prot(std::string("array") + pointerToString(this)), container()
			{
				assert ( data.size() == filenames.size() );

				for ( uint64_t i = 0; i < filenames.size(); ++i )
				{
					Vurl.push_back(prot + ":" + filenames[i]);
					container.add(filenames[i],data[i].first,data[i].second);
				}

				// set up factory
				typename libmaus2::aio::ArrayInputStreamFactory<iterator>::shared_ptr_type factory(
					new libmaus2::aio::ArrayInputStreamFactory<iterator>(container));
				// add protocol handler
				libmaus2::aio::InputStreamFactoryContainer::addHandler(prot, factory);
			}

			~ArrayFileSet()
			{
				libmaus2::aio::InputStreamFactoryContainer::removeHandler(prot);
			}

			std::string getURL(uint64_t const id) const
			{
				return Vurl[id];
			}
		};
	}
}
#endif
