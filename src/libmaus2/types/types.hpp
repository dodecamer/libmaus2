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

#if ! defined(LIBMAUS2_TYPES_TYPES_HPP)
#define LIBMAUS2_TYPES_TYPES_HPP

#if defined(__GNUC__) && __GNUC__ >= 3
#define expect_true(x)      __builtin_expect (x, 1)
#define expect_false(x)     __builtin_expect (x, 0)
#else
#define expect_true(x) x
#define expect_false(x) x
#endif

#if defined(__GNUC__) && (__GNUC__ >= 7) && ! defined(__clang__)

#if __cplusplus <= 199711L // up to C++03
#define libmaus2_fallthrough __attribute__ ((fallthrough))
#else

#if __cplusplus >= 201703L
#define libmaus2_fallthrough [[fallthrough]]
#else
#define libmaus2_fallthrough [[gnu::fallthrough]]
#endif

#endif // __cplusplus <= 199711L

#else

#define libmaus2_fallthrough

#endif // __GNUC__

#include <libmaus2/LibMausConfig.hpp>
#include <cstdlib>

#if defined(LIBMAUS2_HAVE_CSTDINT) || defined(_MSC_VER)
#include <cstdint>
#elif defined(LIBMAUS2_HAVE_STDINT_H)
#include <stdint.h>
#elif defined(LIBMAUS2_HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

#if defined(__APPLE__)
#include <stdint.h>
#endif

#if defined(LIBMAUS2_HAVE_UNSIGNED_INT128)
namespace libmaus2
{
	typedef unsigned __int128 uint128_t;
}
#endif

#endif
