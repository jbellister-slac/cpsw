#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <cpsw_api_user.h>

#include <boost/weak_ptr.hpp>
#include <boost/atomic.hpp>
#include <stdio.h>

#include <cpsw_buf.h>
#include <cpsw_shared_obj.h>
#include <cpsw_event.h>

using boost::weak_ptr;

class IProtoPort;
typedef shared_ptr<IProtoPort> ProtoPort;

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class CPortImpl;
typedef shared_ptr<CPortImpl> ProtoPortImpl;

class ProtoPortMatchParams;

namespace YAML {
	class Node;
}

class IProtoPort {
public:

	static const bool         ABS_TIMEOUT = true;
	static const bool         REL_TIMEOUT = false;

	// If a port is taken offline then all traffic is dropped

	// NOTE: the 'offline' feature is a late addition and
	//       ugly. We should really have a proper 'open' which
	//       gives back a handle to an interface with methods
	//       possible on an open port (push/pop).
	//       This would imply a minor redesign of the ProtoPort
	//       which I can't afford ATM.
	virtual bool      isOffline()                      const = 0;
	virtual void      setOffline(bool)                       = 0;

	// returns NULL shared_ptr on timeout; throws on error
	virtual BufChain pop(const CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                                = 0;

	// Successfully pushed buffers are unlinked from the chain
	virtual bool push(BufChain , const CTimeout *, bool abs_timeout) = 0;
	virtual bool tryPush(BufChain)                           = 0;


	virtual ProtoMod  getProtoMod()                          = 0;
	virtual ProtoPort getUpstreamPort()                      = 0;

	virtual void      addAtPort(ProtoMod downstreamMod)      = 0;

	virtual IEventSource *getReadEventSource()               = 0;

	virtual CTimeout  getAbsTimeoutPop (const CTimeout *)    = 0;
	virtual CTimeout  getAbsTimeoutPush(const CTimeout *)    = 0;

	virtual void      dumpYaml(YAML::Node &)     const       = 0;

	virtual int       match(ProtoPortMatchParams*)           = 0;

	virtual ~IProtoPort()
	{
	}
};

// find a protocol stack based on parameters
class ProtoPortMatchParams {
public:
	class MatchParam {
	public:
		ProtoPort matchedBy_;
		bool      doMatch_;
		bool      exclude_;
		ProtoMod  handledBy_;
		MatchParam(bool doMatch = false)
		: doMatch_(doMatch),
		  exclude_(false)
		{
		}
		void exclude()
		{
			doMatch_ = true;
			exclude_ = true;
		}
		void include()
		{
			doMatch_ = true;
			exclude_ = false;
		}

		int excluded()
		{
			return doMatch_ && exclude_ && ! handledBy_ ? 1 : 0;
		}

		void reset()
		{
			matchedBy_.reset();
			handledBy_.reset();
		}
	};

	class MatchParamUnsigned : public MatchParam {
	protected:
		unsigned val_;
		bool     any_;
	public:
		MatchParamUnsigned(unsigned val = (unsigned)-1, bool doMatch = false)
		: MatchParam( doMatch ? true : val != (unsigned)-1 ),
		  val_(val),
		  any_(false)
		{
		}

		void wildcard()
		{
			any_     = true;
			include();
		}

		MatchParamUnsigned & operator=(unsigned val)
		{
			val_     = val;
			include();
			any_     = false;
			return *this;
		}

		bool operator==(unsigned val)
		{
			return doMatch_ && (any_ || (val_ == val));
		}

	};
	MatchParamUnsigned udpDestPort_, srpVersion_, srpVC_, tDest_;
	MatchParam         haveRssi_, haveDepack_;

	void reset()
	{
		udpDestPort_.reset();
		srpVersion_.reset();
		srpVC_.reset();
		tDest_.reset();
		haveRssi_.reset();
		haveDepack_.reset();
	}

	int requestedMatches()
	{
	int rval = 0;
		if ( udpDestPort_.doMatch_ )
			rval++;
		if ( haveDepack_.doMatch_ )
			rval++;
		if ( srpVersion_.doMatch_ )
			rval++;
		if ( srpVC_.doMatch_ )
			rval++;
		if ( haveRssi_.doMatch_ )
			rval++;
		if ( tDest_.doMatch_ )
			rval++;
		return rval;
	}

	int excluded()
	{
		return
			  udpDestPort_.excluded()
			+ haveDepack_.excluded()
			+ srpVersion_.excluded()
			+ srpVC_.excluded()
			+ haveRssi_.excluded()
			+ tDest_.excluded();
	}

	int findMatches(ProtoPort p)
	{
		int rval  = p->match( this );
			rval += excluded();

		return rval;
	}
};


class IProtoMod {
public:
	// to be called by the upstream module's addAtPort() method
	// (which is protocol specific)
	virtual void attach(ProtoPort upstream)            = 0;

	virtual ProtoPort getUpstreamPort()                = 0;
	virtual ProtoMod  getUpstreamProtoMod()            = 0;

	virtual void dumpInfo(FILE *)                      = 0;

	virtual void modStartup()                          = 0;
	virtual void modShutdown()                         = 0;

	virtual const char *getName() const                = 0;

	virtual ~IProtoMod() {}
};

class IPortImpl : public IProtoPort {
private:
	boost::atomic<bool> offline_;

public:
	virtual int iMatch(ProtoPortMatchParams *cmp) = 0;

public:
	IPortImpl();

	virtual int match(ProtoPortMatchParams *cmp);

	virtual bool isOffline() const;

	virtual void setOffline(bool offline);
};

class CPortImpl : public IPortImpl {
private:
	weak_ptr< ProtoMod::element_type > downstream_;
	BufQueue                           outputQueue_;
	unsigned                           depth_;

protected:

	CPortImpl(const CPortImpl &orig)
	{
		// would have to set downstream_ to
		// the respective clone and clone
		// the output queue...
		throw InternalError("clone not implemented");
	}

	virtual BufChain processOutput(BufChain bc)
	{
		throw InternalError("processOutput() not implemented!\n");
	}

	virtual BufChain processInput(BufChain bc)
	{
		throw InternalError("processInput() not implemented!\n");
	}

	virtual ProtoPort getSelfAsProtoPort()              = 0;

public:
	CPortImpl(unsigned n);

	virtual unsigned getQueueDepth() const;

	virtual IEventSource *getReadEventSource();

	virtual ProtoPort mustGetUpstreamPort();

	virtual void addAtPort(ProtoMod downstream);

	virtual BufChain pop(const CTimeout *timeout, bool abs_timeout);

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

	// getAbsTimeout is not a member of the CTimeout class:
	// the clock to be used is implementation dependent.
	// ProtoMod uses a semaphore which uses CLOCK_REALTIME.
	// The conversion to abs-time should be a member
	// of the same class which uses the clock-dependent
	// resource...
	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout);

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout);

	virtual BufChain tryPop();

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout);

	virtual bool tryPush(BufChain bc);

	virtual ProtoPort getUpstreamPort();

	virtual ~CPortImpl()
	{
	}
};

class CProtoModImpl : public IProtoMod {
protected:
	ProtoPort  upstream_;
	CProtoModImpl(const CProtoModImpl &orig);

public:
	CProtoModImpl();

	// subclasses may want to start threads
	// once the module is attached
	virtual void modStartup();

	virtual void modShutdown();

	virtual void attach(ProtoPort upstream);

	virtual ProtoPort getUpstreamPort();

	virtual ProtoMod  getUpstreamProtoMod();

	virtual void dumpInfo(FILE *f);
};

// protocol module with single downstream port
class CProtoMod : public CShObj, public CProtoModImpl, public CPortImpl {

protected:
	CProtoMod(const CProtoMod &orig, Key &k);

	virtual ProtoPort getSelfAsProtoPort();

public:
	CProtoMod(Key &k, unsigned n);

	virtual bool pushDown(BufChain bc, const CTimeout *rel_timeout);

public:

	virtual ProtoMod getProtoMod();

	virtual ~CProtoMod()
	{
	}
};

#endif
