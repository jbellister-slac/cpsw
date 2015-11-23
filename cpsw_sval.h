#ifndef CPSW_SVAL_H
#define CPSW_SVAL_H

#include <cpsw_api_builder.h>
#include <cpsw_entry.h>

using boost::static_pointer_cast;

class CIntEntryImpl;
typedef shared_ptr<CIntEntryImpl> IntEntryImpl;

class CScalVal_ROAdapt;
typedef shared_ptr<CScalVal_ROAdapt> ScalVal_ROAdapt;
class CScalVal_WOAdapt;
typedef shared_ptr<CScalVal_WOAdapt> ScalVal_WOAdapt;
class CScalVal_Adapt;
typedef shared_ptr<CScalVal_Adapt>   ScalVal_Adapt;

class IEntryAdapt : public virtual IEntry {
protected:
	shared_ptr<const CEntryImpl> ie;
	Path     p;

protected:
	IEntryAdapt(Path p, shared_ptr<const CEntryImpl> ie);

public:
	virtual const char *getName()        const { return ie->getName(); }
	virtual const char *getDescription() const { return ie->getDescription(); }
	virtual uint64_t    getSize()        const { return ie->getSize(); }
};

class CIntEntryImpl : public CEntryImpl, public virtual IIntField {
private:
	bool     is_signed;
	int      ls_bit;
	uint64_t size_bits;
	Mode     mode;
	unsigned wordSwap;
public:
	CIntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit = 0, Mode mode = RW, unsigned wordSwap = 0);

	virtual bool     isSigned()    const { return is_signed; }
	virtual int      getLsBit()    const { return ls_bit;    }
	virtual uint64_t getSizeBits() const { return size_bits; }
	virtual unsigned getWordSwap() const { return wordSwap;  }
	virtual Mode     getMode()     const { return mode;      }
};

class IIntEntryAdapt : public IEntryAdapt, public virtual IScalVal_Base {
private:
	int nelms;
public:
	IIntEntryAdapt(Path p, shared_ptr<const CIntEntryImpl> ie) : IEntryAdapt(p, ie), nelms(-1) {}
	virtual bool     isSigned()    const { return asIntEntry()->isSigned();    }
	virtual int      getLsBit()    const { return asIntEntry()->getLsBit();    }
	virtual uint64_t getSizeBits() const { return asIntEntry()->getSizeBits(); }
	virtual unsigned getWordSwap() const { return asIntEntry()->getWordSwap(); }
	virtual IIntField::Mode     getMode()     const { return asIntEntry()->getMode(); }
	virtual unsigned getNelms();

protected:
	virtual shared_ptr<const CIntEntryImpl> asIntEntry() const { return static_pointer_cast<const CIntEntryImpl, const CEntryImpl>(ie); }

};

class CScalVal_ROAdapt : public virtual IScalVal_RO, public virtual IIntEntryAdapt {
public:
	CScalVal_ROAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie)
	{
	}

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned);

	template <typename E> unsigned getVal(E *e, unsigned nelms) {
		return getVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E) );
	}

	virtual unsigned getVal(uint64_t *p, unsigned n) { return getVal<uint64_t>(p,n); }
	virtual unsigned getVal(uint32_t *p, unsigned n) { return getVal<uint32_t>(p,n); }
	virtual unsigned getVal(uint16_t *p, unsigned n) { return getVal<uint16_t>(p,n); }
	virtual unsigned getVal(uint8_t  *p, unsigned n) { return getVal<uint8_t> (p,n); }

};

class CScalVal_WOAdapt : public virtual IScalVal_WO, public virtual IIntEntryAdapt {
public:
	CScalVal_WOAdapt(Path p, shared_ptr<const CIntEntryImpl> ie);

	template <typename E> unsigned setVal(E *e, unsigned nelms) {
		return setVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E) );
	}

	virtual unsigned setVal(uint8_t  *, unsigned, unsigned);

	virtual unsigned setVal(uint64_t *p, unsigned n) { return setVal<uint64_t>(p,n); }
	virtual unsigned setVal(uint32_t *p, unsigned n) { return setVal<uint32_t>(p,n); }
	virtual unsigned setVal(uint16_t *p, unsigned n) { return setVal<uint16_t>(p,n); }
	virtual unsigned setVal(uint8_t  *p, unsigned n) { return setVal<uint8_t> (p,n); }

	virtual unsigned setVal(uint64_t  v);
};

class CScalVal_Adapt : public virtual CScalVal_ROAdapt, public virtual CScalVal_WOAdapt, public virtual IScalVal {
public:
	CScalVal_Adapt(Path p, shared_ptr<const CIntEntryImpl> ie);
};

#endif
