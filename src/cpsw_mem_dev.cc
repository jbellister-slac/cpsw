 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <cpsw_yaml.h>

#undef  MEMDEV_DEBUG

CMemDevImpl::CMemDevImpl(Key &k, const char *name, uint64_t size, uint8_t *ext_buf)
: CDevImpl(k, name, size),
  buf_    ( ext_buf ? ext_buf : 0  ),
  isExt_  ( ext_buf ? true : false )
{
	if ( ! buf_ && size ) {
		buf_ = (uint8_t*)mmap( 0, getSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );

		if ( MAP_FAILED == buf_ ) {
			throw InternalError("CMemDevImpl - Unable to map anonymous buffer", errno);
		}
	}
}

CMemDevImpl::CMemDevImpl(Key &key, YamlState &node)
: CDevImpl( key, node ),
  buf_    ( 0         ),
  isExt_  ( false     )
{
int flg = MAP_PRIVATE | MAP_ANONYMOUS;
int fd  = -1;
int err;
	if ( 0 == size_ ) {
		throw InvalidArgError("'size' zero or unset");
	}
	if ( readNode( node, YAML_KEY_fileName, &fileName_ ) ) {
        if ( (fd = open( fileName_.c_str(), O_RDWR )) < 0 ) {
			throw InternalError( std::string("CMemDevImpl - Unable to open") + fileName_, errno );
		}
		flg = MAP_SHARED;
	}
	buf_ = (uint8_t*)mmap( 0, getSize(), PROT_READ | PROT_WRITE, flg, fd, 0 );
    err  = errno;
	if ( fd >= 0 ) {
		close( fd );
	}
	if ( MAP_FAILED == buf_ ) {
		throw InternalError("CMemDevImpl - Unable to map anonymous buffer", err);
	}
}


CMemDevImpl::CMemDevImpl(const CMemDevImpl &orig, Key &k)
: CDevImpl( orig,     k ),
  buf_    ( orig.buf_   ),
  isExt_  ( orig.isExt_ )
{
	if ( ! orig.isExt_ ) {
		buf_ = (uint8_t*)mmap( 0, getSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
		if ( MAP_FAILED == buf_ ) {
			throw InternalError( "CMemDevImpl - Unable to map anonymous buffer", errno );
		}
		memcpy( buf_, orig.buf_, orig.getSize() );
	}
}

CMemDevImpl::~CMemDevImpl()
{
	if ( ! isExt_ )
		munmap( buf_, size_ );
	/* let a shared mapping live on */
}

void CMemDevImpl::addAtAddress(Field child)
{
IAddress::AKey k = getAKey();

	add( cpsw::make_shared<CMemAddressImpl>(k), child );
}

void CMemDevImpl::addAtAddress(Field child, unsigned nelms)
{
	if ( 1 != nelms )
		throw ConfigurationError("CMemDevImpl::addAtAddress -- can only have exactly 1 child");
	addAtAddress( child );
}

void CMemDevImpl::dumpYamlPart(YAML::Node &node) const
{
	CDevImpl::dumpYamlPart( node );
	if ( fileName_.size() != 0 ) {
		writeNode( node, YAML_KEY_fileName, fileName_ );
	}
}

CMemAddressImpl::CMemAddressImpl(AKey k)
: CAddressImpl(k, 1)
{
}

uint64_t CMemAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
unsigned toget = args->nbytes_;
	if ( args->off_ + toget > owner->getSize() ) {
#ifdef MEMDEV_DEBUG
printf("off %lu, dbytes %lu, size %lu\n", args->off_, args->nbytes_, owner->getSize());
#endif
		throw ConfigurationError("MemAddress: read out of range");
	}
	if ( ! args->dst_  ) {
		// 'peeking' read, i.e., the user wants to see if data are ready;
		// at least 1 byte they can
		return 1;
	}
	memcpy(args->dst_, owner->getBufp() + args->off_, toget);
#ifdef MEMDEV_DEBUG
printf("MemDev read from off %lli to %p:", args->off_, args->dst_);
for ( unsigned ii=0; ii<args->nbytes_; ii++) printf(" 0x%02x", args->dst_[ii]); printf("\n");
#endif
	if ( args->aio_ )
		args->aio_->callback( 0 );
	return toget;
}

uint64_t CMemAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
uint8_t *buf  = owner->getBufp();
unsigned put  = args->nbytes_;
unsigned rval = put;
uint8_t  msk1 = args->msk1_;
uint8_t  mskn = args->mskn_;
uint64_t off  = args->off_;
uint8_t *src  = args->src_;

	if ( off + put > owner->getSize() ) {
		throw ConfigurationError("MemAddress: write out of range");
	}

#ifdef MEMDEV_DEBUG
printf("MemDev write to off %lli from %p:", args->off_, args->src_);
for ( unsigned ii=0; ii<args->nbytes_; ii++) printf(" 0x%02x", args->src_[ii]); printf("\n");
#endif

	if ( (msk1 || mskn) && put == 1 ) {
		msk1 |= mskn;
		mskn  = 0;
	}

	if ( msk1 ) {
		/* merge 1st byte */
		buf[off] = ( (buf[off]) & msk1 ) | ( src[0] & ~msk1 ) ;
		off++;
		put--;
		src++;
	}

	if ( mskn ) {
		put--;
	}
	if ( put ) {
		memcpy(owner->getBufp() + off, src, put);
	}
	if ( mskn ) {
		/* merge last byte */
		buf[off+put] = (buf[off+put] & mskn ) | ( src[put] & ~mskn );
	}
	return rval;
}

MemDev IMemDev::create(const char *name, uint64_t size, uint8_t *ext_buf)
{
	return CShObj::create<MemDevImpl>(name, size, ext_buf);
}
