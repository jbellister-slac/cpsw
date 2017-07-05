 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.


#include <cpsw_sock.h>
#include <cpsw_error.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

CSockSd::CSockSd(int type)
: sd_(-1),
  type_(type),
  dest_(NULL),
  nblk_(false),
  isConn_(false)
{
	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
	me_.sin_family      = AF_INET;
	me_.sin_addr.s_addr = INADDR_ANY;
	me_.sin_port        = htons( 0 );
}

void CSockSd::getMyAddr(struct sockaddr_in *addr_p)
{
	socklen_t l = sizeof(*addr_p);
	if ( getsockname( getSd(), (struct sockaddr*)addr_p, &l ) ) {
		throw IOError("getsockname() ", errno);
	}
}

CSockSd::CSockSd(CSockSd &orig)
: sd_(-1),
  type_(orig.type_),
  me_(orig.me_),
  dest_(orig.dest_ ? new struct sockaddr_in(*orig.dest_) : NULL),
  nblk_(orig.nblk_),
  isConn_(orig.isConn_)
{
	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
	if ( isConn_ ) {
		init( dest_, NULL, nblk_ );
	}
}

void CSockSd::reconnect()
{
	close( sd_ );
	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
	if ( isConn_ ) {
		init( dest_, NULL, nblk_ );
	}
}

void CSockSd::init(struct sockaddr_in *dest, struct sockaddr_in *me_p, bool nblk)
{
	int    optval;

	if ( me_p ) {
		me_ = *me_p;
	}

	if ( (nblk_ = nblk) ) {
		if ( ::fcntl( sd_, F_SETFL, O_NONBLOCK ) ) {
			throw IOError("fcntl(O_NONBLOCK) ", errno);
		}
	}

	optval = 1;
	if ( ::setsockopt(  sd_,  SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) ) {
		throw IOError("setsockopt(SO_REUSEADDR) ", errno);
	}

	optval = 1;
	if ( SOCK_STREAM == type_ ) {
		if ( ::setsockopt( sd_,  IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval) ) ) {
			throw IOError("setsockopt(TCP_NODELAY) ", errno);
		}
	}

	if ( ::bind( sd_, (struct sockaddr*)&me_, sizeof(me_)) ) {
		throw IOError("bind failed ", errno);
	}

	// connect - filters any traffic from other destinations/fpgas in the kernel
	if ( dest ) {
		struct sockaddr_in *tmp = dest_;

		dest_ = new struct sockaddr_in(*dest);
		// they may pass in dest_ which must not be deleted until now
		if ( tmp )
			delete tmp;
	}

	if ( dest_ ) {
		if ( ::connect( sd_, (struct sockaddr*)dest_, sizeof(*dest_) ) )
			throw IOError("connect failed ", errno);
printf("CONNECTED to %s:%d\n", inet_ntoa(dest_->sin_addr), ntohs(dest_->sin_port));
	}

	isConn_ = true;
}

CSockSd::~CSockSd()
{
	close( sd_ );
	if ( dest_ )
		delete dest_;
}
