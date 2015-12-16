#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_proto_mod.h>

#include <errno.h>

#define QUEUE_PUSH_RETRY_INTERVAL 1000 //nano-seconds

CBufQueue::CBufQueue(size_type n)
: queue< IBufChain*, boost::lockfree::fixed_sized< true > >(n)
{
	if ( sem_init( &rd_sem_, 0, 0 ) ) {
		throw InternalError("Unable to create semaphore");
	}
}

CBufQueue::~CBufQueue()
{
	// since there are raw pointers stored in the queue
	// we must extract the shared_ptr ownership from all
	// stored elements here.
	while ( tryPop() ) {
		;
	}
	sem_destroy( &rd_sem_ );
}

bool CBufQueue::push(BufChain *owner)
{
// keep a ref in case we have to undo
BufChain ref = *owner;

	IBufChain::take_ownership(owner);
	// 1 ref inside the BufChain obj
	// 1 ref in our local var
	// (*owner) has been reset

	if ( bounded_push( ref.get() ) ) {
		if ( sem_post( &rd_sem_ ) ) {
			throw InternalError("FATAL ERROR -- unable to post semaphore");
			// cannot easily clean up since the item is in the queue already
		}
		return true;
	} else {
		// push failed. Let's delay this thread for a little while
		// (since we cannot rely on priority scheduling). This might
		// give the consumer the chance to remove some items...
if ( 1 ) {
		struct timespec dely;
		dely.tv_sec  = 0;
		dely.tv_nsec = QUEUE_PUSH_RETRY_INTERVAL;
		nanosleep( &dely, 0 );
} else {
		sched_yield();
}

		if ( bounded_push( ref.get() ) ) {
			if ( sem_post( &rd_sem_ ) ) {
				throw InternalError("FATAL ERROR -- unable to post semaphore");
				// cannot easily clean up since the item is in the queue already
			}
			return true;
		}
		// enqueue failed -- re-transfer smart pointer
		// to owner...
		*owner = ref->yield_ownership();
	}
	return false;
}

BufChain CBufQueue::pop(bool wait, struct timespec *abs_timeout)
{
int sem_stat = wait ?
	                   ( abs_timeout ? sem_timedwait( &rd_sem_, abs_timeout )
	                                 : sem_wait( &rd_sem_ ) )
                    : sem_trywait( &rd_sem_ );

	if ( 0 == sem_stat ) {
		IBufChain *raw_ptr;
		if ( !CBufQueueBase::pop( raw_ptr ) ) {
			throw InternalError("FATAL ERROR -- unable to pop even though we decremented the semaphore?");
		}
		return raw_ptr->yield_ownership();
	}

	switch ( errno ) {
		case EAGAIN:
		case ETIMEDOUT:
			return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
		case EINVAL:
			throw InvalidArgError("invalid timeout arg");
		case EINTR:
			throw IntrError("interrupted by signal");
		default:
			break;
	}

	throw IOError("sem__xxwait failed");
}

BufChain CBufQueue::pop(struct timespec *timeout)
{
	return pop( true, timeout );
}

BufChain CBufQueue::tryPop()
{
	return pop(false, 0);
}

ProtoMod CProtoMod::pushMod( ProtoMod *m_p )
{
	if ( upstream_ )
		throw InternalError("Pushee already attached!");
	upstream_ = *m_p;
	return ( *m_p      = ProtoMod( self_ ) );
}

ProtoMod CProtoMod::cloneStack()
{
ProtoMod rval;
	if ( upstream_ ) {
		rval = upstream_->cloneStack();
	}
	return clone()->pushMod( &rval );
}

ProtoMod CProtoMod::clone()
{
	throw InternalError("Clone not implemented");
}


CTimeout CProtoMod::getAbsTimeout(CTimeout *rel_timeout)
{
	struct timespec ts;

	if ( ! rel_timeout || rel_timeout->isIndefinite() )
		return TIMEOUT_INDEFINITE;

	if ( clock_gettime( CLOCK_REALTIME, &ts ) )
		throw InternalError("clock_gettime failed", errno);

	CTimeout rval( ts );

	if ( ! rel_timeout->isNone() ) {
		rval += *rel_timeout;
	}
	return rval;
}


