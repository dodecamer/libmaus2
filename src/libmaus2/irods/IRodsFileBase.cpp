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
#include <libmaus2/irods/IRodsFileBase.hpp>

libmaus2::irods::IRodsFileBase::IRodsFileBase() : fdvalid(false)
{
}

libmaus2::irods::IRodsFileBase::~IRodsFileBase()
{
	#if defined(LIBMAUS2_HAVE_IRODS)
	if ( fdvalid )
	{
		openedDataObjInp_t closeHandle;
		memset (&closeHandle, 0, sizeof (closeHandle));
		closeHandle.l1descInx = fd;

		int const status = rcDataObjClose(comm, &closeHandle);

		if ( status < 0 )
		{
			char * subname = 0;
			char * name = rodsErrorName(status,&subname);
			std::cerr << "IRodsFileBase::~IRodsFileBase: failed to close data object: " << status << " (" << name << ")" << std::endl;
		}
	}
	#endif
}

// read block of data
uint64_t libmaus2::irods::IRodsFileBase::read(
	char *
		#if defined(LIBMAUS2_HAVE_IRODS)
		buffer
		#endif
		,
	uint64_t
		#if defined(LIBMAUS2_HAVE_IRODS)
		len
		#endif
)
{
	#if defined(LIBMAUS2_HAVE_IRODS)
	openedDataObjInp_t readInHandle;
	bytesBuf_t readOutHandle;

	memset(&readInHandle, 0, sizeof (readInHandle));
	readInHandle.l1descInx = fd;
	readInHandle.len       = len;
	readOutHandle.buf   = buffer;
	readOutHandle.len   = len;

	long const status = rcDataObjRead(comm, &readInHandle, &readOutHandle);

	if ( status < 0 )
	{
		char * subname = 0;
		char * name = rodsErrorName(status,&subname);

		libmaus2::exception::LibMausException lme;
		lme.getStream() << "IRodsFileBase::read(): failed with status " << status << " (" << name << ")" << std::endl;
		lme.finish();
		throw lme;
	}

	return status;
	#else
	return 0;
	#endif
}

// perform seek operation and return new position in stream
uint64_t libmaus2::irods::IRodsFileBase::seek(
	long
		#if defined(LIBMAUS2_HAVE_IRODS)
		offset
		#endif
		,
	int
		#if defined(LIBMAUS2_HAVE_IRODS)
		whence
		#endif
)
{
	#if defined(LIBMAUS2_HAVE_IRODS)
	openedDataObjInp_t seekInput;
	fileLseekOut_t* seekOutput = NULL;
	int status = -1;

	memset(&seekInput, 0, sizeof(seekInput));
	seekInput.l1descInx = fd;
	seekInput.offset    = offset;
	seekInput.whence    = whence;
	if((status = rcDataObjLseek(comm, &seekInput, &seekOutput)) < 0)
	{
		char * subname = 0;
		char * name = rodsErrorName(status,&subname);

		libmaus2::exception::LibMausException lme;
		lme.getStream() << "IRodsFileBase::seek(): failed with status " << status << " (" << name << ")" << std::endl;
		lme.finish();
		throw lme;
	}

	return seekOutput->offset;
	#else
	return 0;
	#endif
}

uint64_t libmaus2::irods::IRodsFileBase::size()
{
	uint64_t const curpos = seek(0,SEEK_CUR);
	uint64_t const filesize = seek(0,SEEK_END);
	seek(curpos,SEEK_SET);
	return filesize;
}
