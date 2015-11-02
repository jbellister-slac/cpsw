#ifndef CPSW_API_USER_H
#define CPSW_API_USER_H

#include <stdint.h>
#include <list>
#include <string.h>
#include <boost/shared_ptr.hpp>

using std::list;
using boost::shared_ptr;

class IDev;
class Child;
class IPath;
class IScalVal_RO;
class IScalVal;
class CompositePathIterator;
typedef shared_ptr<IPath>  Path;
typedef shared_ptr<IScalVal_RO> ScalVal_RO;
typedef shared_ptr<IScalVal>    ScalVal;
class EventListener;

#include <cpsw_error.h>

// The hierarchy of things

// A node 
class IEntry {
public:
	virtual const char *getName()  const = 0;
	virtual uint64_t getSizeBits() const = 0;
	virtual ~IEntry() {}
};

class IPath {
public:
	// lookup 'name' under this path and return new 'full' path
	virtual Path        findByName(const char *name) const = 0;
	// strip last element of this path
	virtual void        up()                         = 0;
	// test if this path is empty
	virtual bool        empty()                const = 0;
	virtual void        clear()                      = 0; // absolute; reset to root
	virtual void        clear(const IDev *)          = 0; // relative; reset to passed arg
	// return IDev at the tip of this path (if any -- NULL otherwise)
	virtual const IDev *origin()               const = 0;
	// return parent IDev (if any -- NULL otherwise )
	virtual const IDev *parent()               const = 0;
	// return Entry at the end of this path (if any -- NULL otherwise)
	virtual const IEntry *tail()               const = 0;
	virtual std::string toString()             const = 0;
	virtual void        dump(FILE *)           const = 0;
	// verify the 'hub' is at the tail of this path
	virtual bool        verifyAtTail(const IDev *)   = 0;
	// append a copy of another path to this one.
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual void        append(Path p)               = 0;
	// append to this path
	// Note: an exception is thrown if this->verifyAtTail( child->owner() ) evaluates to 'false'.
	virtual void        append(Child *c)             = 0;
	
	// append a copy of another path to a copy of this one and return the new copy
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual Path        concat(Path p)         const = 0;

	// create an empty path
	static  Path        create();             // absolute; starting at root
	static  Path        create(const IDev *);  // relative; starting at passed arg

	virtual ~IPath() {}
};

// A collection of nodes
class IDev : public virtual IEntry {
public:
	// find all entries matching 'path' in or underneath this hub
	virtual Path           findByName(const char *path)   const = 0;

	virtual const Child   *getChild(const char *name)     const = 0;
};

// Read-only interface to an integral value.
// Any size (1..64 bits) is represented by
// a (sign-extended) uint.

class IScalVal_RO : public virtual IEntry {
public:
	virtual int      getNelms()               = 0; // if array
	virtual unsigned getVal(uint64_t *p, unsigned nelms)      = 0; // (possibly truncating, sign-extending) read into 64-bit (array)
	virtual unsigned getVal(uint32_t *p, unsigned nelms)      = 0; // (possibly truncating, sign-extending) read into 32-bit (array)
	virtual unsigned getVal(uint16_t *p, unsigned nelms)      = 0; // (possibly truncating, sign-extending) read into 16-bit (array)
	virtual unsigned getVal(uint8_t  *p, unsigned nelms)      = 0; // (possibly truncating, sign-extending) read into  8-bit (array)
	virtual bool     isSigned()         const = 0;
	virtual ~IScalVal_RO () {}

	// Throws an exception if path doesn't point to an object which supports this interface
	static ScalVal_RO create(Path p);
};

// Write-only: not provided ATM; caching may be required anyways

// Read-write
class IScalVal : public IScalVal_RO {
public:
	virtual void     setVal(uint64_t *p) = 0; // (possibly truncating) write from 64-bit value(s)
	virtual void     setVal(uint32_t *p) = 0; // (possibly truncating) write from 32-bit value(s)
	virtual void     setVal(uint16_t *p) = 0; // (possibly truncating) write from 16-bit value(s)
	virtual void     setVal(uint8_t  *p) = 0; // (possibly truncating) write from  8-bit value(s)
	virtual void     setVal(uint64_t  v) = 0; // set all elements to same value
	virtual ~IScalVal () {}

	// Throws an exception if path doesn't point to an object which supports this interface
	static ScalVal create(Path p);
};

// Analogous to ScalVal there could be interfaces to double, enums, strings...


// Event AKA Interrupt interface

class EventSource {
public:
	class Event {
		public:
		virtual ~Event() {};
	};
	virtual void addListener(Event *, EventListener *)    = 0;
	virtual void removeListener(Event *, EventListener *) = 0;
	virtual ~EventSource() {}
};

class EventListener {
public:
	virtual void notify(EventSource::Event *e) = 0;
	virtual ~EventListener() {}
};

#endif
