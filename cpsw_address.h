#ifndef CPSW_ADDRESS_H
#define CPSW_ADDRESS_H

#include <cpsw_api_builder.h>
#include <cpsw_entry.h>

#include <stdio.h>
//#include <cpsw_hub.h>

#include <boost/weak_ptr.hpp>
using boost::weak_ptr;

class   CDevImpl;
class   CAddressImpl;
class   CompositePathIterator;
class   IAddress;

typedef shared_ptr<CDevImpl> DevImpl;
typedef weak_ptr<CDevImpl>   WDevImpl;
typedef shared_ptr<IAddress> Address;
typedef shared_ptr<CAddressImpl> AddressImpl;

class CReadArgs {
public:
	IField::Cacheable cacheable_;
	uint8_t          *dst_;
	unsigned          nbytes_;
	uint64_t          off_;
	CTimeout          timeout_;
	CReadArgs()
	: cacheable_ ( IField::UNKNOWN_CACHEABLE ),
	  dst_       ( NULL ),
	  nbytes_    ( 0 ),
	  off_       ( 0 ),
	  timeout_   ( TIMEOUT_INDEFINITE )
	{
	}
};

class CWriteArgs {
public:
	IField::Cacheable cacheable_;
	uint8_t          *src_;
	uint64_t          off_;
	unsigned          nbytes_;
	uint8_t           msk1_;
	uint8_t           mskn_;
	CTimeout          timeout_;
	CWriteArgs()
	: cacheable_ ( IField::UNKNOWN_CACHEABLE ),
	  src_       ( NULL ),
	  off_       ( 0 ),
	  nbytes_    ( 0 ),
	  msk1_      ( 0 ),
	  mskn_      ( 0 ),
	  timeout_   ( TIMEOUT_INDEFINITE )
	{
	}
};


class IAddress : public IChild {
	public:
		class AKey {
			private:
				WDevImpl owner_;
				AKey(DevImpl owner):owner_(owner) {}
			public:
				const DevImpl get() const { return DevImpl(owner_); }

				template <typename T> T getAs() const
				{
					return static_pointer_cast<typename T::element_type, DevImpl::element_type>( get() );
				}

				friend class CDevImpl;
				friend class CAddressImpl;
		};

		virtual void attach(EntryImpl child) = 0;

		virtual uint64_t read (CompositePathIterator *node, CReadArgs *args)  const = 0;
		virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const = 0;

		virtual void dump(FILE *f)           const = 0;

		virtual void dump()                  const = 0;

		virtual EntryImpl getEntryImpl()     const = 0;

		virtual DevImpl getOwnerAsDevImpl()  const = 0;

		virtual ByteOrder getByteOrder()     const = 0;

		virtual void dumpYamlPart(YAML::Node &) const = 0;

		// EVERY subclass 'XAddr' MUST implement
		//
		// private:
		//    virtual XAddr *clone() { return new XAddr( *this ); }
		//
		virtual IAddress * clone(AKey) = 0;

		virtual Address clone(DevImpl) = 0;

		virtual ~IAddress() {}
};

#define NULLCHILD Child( static_cast<IChild *>(NULL) )
#define NULLADDR  Address( static_cast<IAddress *>(NULL) )

class CAddressImpl : public IAddress {
	protected:
		mutable AKey       owner_;
	private:
		mutable EntryImpl  child_;
		unsigned           nelms_;
	protected:
		ByteOrder      byteOrder_;

	protected:

		CAddressImpl(const CAddressImpl&, AKey new_owner);

	private:
		CAddressImpl & operator=(const CAddressImpl &);
		CAddressImpl(const CAddressImpl&);

	public:
		CAddressImpl(AKey owner, unsigned nelms = 1, ByteOrder byteOrder = UNKNOWN);

		virtual ~CAddressImpl()
		{
		}

		virtual void attach(EntryImpl child);

		virtual Entry getEntry() const
		{
			return child_;
		}

		virtual EntryImpl getEntryImpl() const
		{
			return child_;
		}

		virtual void dumpYamlPart(YAML::Node &) const;

		virtual const char *getName() const;

		virtual const char *getDescription() const;
		virtual uint64_t    getSize() const;

		virtual unsigned getNelms() const
		{
			return nelms_;
		}

		virtual ByteOrder getByteOrder() const
		{
			return byteOrder_;
		}

		virtual uint64_t read (CompositePathIterator *node, CReadArgs *args) const;
		virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

		virtual void dump(FILE *f) const;

		virtual void dump() const
		{
			dump( stdout );
		}

		virtual Hub     isHub()             const;

		virtual Hub     getOwner()          const;
		virtual DevImpl getOwnerAsDevImpl() const;

		virtual CAddressImpl *clone(AKey k) { return new CAddressImpl( *this, k ); }

		// This should NOT be overridden unless you know what you are doing!
		virtual Address clone(DevImpl);

	protected:
		template <typename T> T getOwnerAs() const
		{
			return static_pointer_cast<typename T::element_type, DevImpl::element_type>( this->getOwnerAsDevImpl() );
		}
};

#endif
