#include <cpsw_api_user.h>
#include <cpsw_yaml.h>
#include <yaml-cpp/yaml.h>
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/list.hpp>
#include <boost/python/dict.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace boost::python;

namespace cpsw_python {

// Translate CPSW Errors/exceptions into python exceptions
//
// This is complicated by the fact that there is no easy
// way to use 'boost::python': the problem is that exceptions
// must be derived from PyErr_Exception (C-API) and AFAIK
// there is no way to tell the boost::python 'class_' template
// that a class is derived from a python C-API object; only
// c++ classes can be bases of class_''.
// If the python object is not a subclass of PyErr_Exception
// then
//
//   try:
//     function() #raises c++ exception
//   except ClassGeneratedBy_class_template as e:
//     handle(e)
//
// works fine if indeed the expected exception is thrown.
// However, if any *other* (wrapped) exception is thrown then python
// complains:
//
//   SystemError: 'finally' pops bad exception
//  
// probably because the class object that was thrown is not
// a PyErr_Exception.
//
// Thus, we must resort to some C-API Kludgery for dealing with
// CPSW Exceptions...
//
// See also:
//
// http://cplusplus-sig.plu5pluscp.narkive.com/uByjeyti/exception-translation-to-python-classes
//
// The first argument to PyErr_NewException must be a 'type' object which is
// derived from PyErr_Exception; AFAIK, boost::python does not support that yet.
// Thus, we must craft such an object ourselves...


template <typename ECL>
class ExceptionTranslator {
private:
	std::string  name_;
	PyObject    *excTypeObj_;

	ExceptionTranslator &operator=(const ExceptionTranslator &orig);

public:

	ExceptionTranslator(const ExceptionTranslator &orig)
	: name_(orig.name_),
	  excTypeObj_(orig.excTypeObj_)
	{
		if ( excTypeObj_ )
			Py_INCREF(excTypeObj_);
	}

	static bool firstTime()
	{
	static bool firstTime_ = true;
		bool rval  = firstTime_;
		firstTime_ = false;
		return rval;
	}

	// obtain exception type (C-API) object - WITHOUT incrementing
	// the reference count.
	PyObject *getTypeObj() const
	{
		return excTypeObj_;
	}

	ExceptionTranslator(const char *name, PyObject *base=0)
	: name_(name),
	  excTypeObj_(0)
	{
		if ( ! firstTime() )
			throw std::exception();

		std::string scopeName = extract<std::string>(scope().attr("__name__"));

		char qualifiedName[ scopeName.size() + name_.size() + 1 + 1 ];
		::strcpy(qualifiedName, scopeName.c_str());
		::strcat(qualifiedName, ".");
		::strcat(qualifiedName, name_.c_str());

		// create a new C-API class which is derived from 'base'
		// (must be a subclass of PyErr_Exception)
		excTypeObj_ = PyErr_NewException( qualifiedName, base, 0 );

		//std::cout << qualifiedName << " typeObj_ refcnt " << Py_REFCNT( excTypeObj_ ) << "\n";

		// Register in the current scope
		scope current;

		current.attr(name_.c_str()) = object( handle<>( borrowed( excTypeObj_ ) ) ); \

     	//std::cout << "EXCEPTION TYPE REFCOUNT CRE " << Py_REFCNT(getTypeObj()) << "\n";
	}

	~ExceptionTranslator()
	{
		//std::cout << "~" << name_ << " typeObj_ refcnt " << Py_REFCNT( getTypeObj() ) << "\n";
		if ( excTypeObj_ )
			Py_DECREF( excTypeObj_ );
	}

	// The actual translation
	void operator() (const ECL &e) const
	{
     //std::cout << "EXCEPTION TYPE REFCOUNT PRE " << Py_REFCNT(getTypeObj()) << "\n";
		PyErr_SetString( getTypeObj(), e.what() );
     //std::cout << "EXCEPTION TYPE REFCOUNT PST " << Py_REFCNT(getTypeObj()) << "\n";
	}

// Macros to save typing
#define ExceptionTranslatorInstall(clazz) \
	ExceptionTranslator<clazz> tr_##clazz(#clazz); \
	register_exception_translator<clazz>( tr_##clazz );

#define ExceptionTranslatorInstallDerived(clazz, base) \
	ExceptionTranslator<clazz> tr_##clazz(#clazz, tr_##base.getTypeObj()); \
	register_exception_translator<clazz>( tr_##clazz );
};

static void wrap_Path_loadConfigFromYamlFile(Path p, const char *filename, const char *yaml_dir)
{
YAML::Node conf( CYamlFieldFactoryBase::loadPreprocessedYamlFile( filename, yaml_dir ) );
	p->loadConfigFromYaml( conf );
}

static void wrap_Path_loadConfigFromYamlString(Path p, const char *yaml,  const char *yaml_dir)
{
YAML::Node conf( CYamlFieldFactoryBase::loadPreprocessedYaml( yaml, yaml_dir ) );
	p->loadConfigFromYaml( conf );
}

// raii for file stream
class ofs : public std::fstream {
public:
	ofs(const char *filename)
	: std::fstream( filename, out )
	{
	}

	~ofs()
	{
		close();
	}
};

static void wrap_Path_dumpConfigToYamlFile(Path p, const char *filename)
{
YAML::Node conf;
	p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

ofs strm( filename );
	strm << emit.c_str() << "\n";
}

static std::string wrap_Path_dumpConfigToYaml(Path p)
{
YAML::Node conf;
	p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

std::ostringstream strm;

	strm << emit.c_str() << "\n";

	return strm.str();
}


// Need wrappers for methods which take 
// shared pointers to const objects which
// do not seem to be handled correctly
// by boost::python.
static void wrap_Path_clear(Path p, shared_ptr<IHub> h)
{
Hub hc(h);
	p->clear(hc);
}

static Path wrap_Path_create(shared_ptr<IHub> h)
{
Hub hc(h);
	return IPath::create(hc);
}

static boost::python::dict wrap_Enum_getItems(Enum enm)
{
boost::python::dict d;

IEnum::iterator it  = enm->begin();
IEnum::iterator ite = enm->end();
	while ( it != ite ) {
		d[ *(*it).first ] = (*it).second;
		++it;
	}

	return d;
}

class ViewGuard {
private:
	Py_buffer *theview_;

public:
	ViewGuard(Py_buffer *view)
	: theview_(view)
	{
	}

	~ViewGuard()
	{
		PyBuffer_Release( theview_ );
	}
};

// Read into an object which implements the (new) python buffer interface
// (supporting only a contiguous buffer)

static unsigned wrap_ScalVal_RO_getVal_into(ScalVal_RO val, object &o, int from, int to)
{
PyObject  *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
Py_buffer  view;
IndexRange rng(from, to);

	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS | PyBUF_WRITEABLE ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	Py_ssize_t nelms = view.len / view.itemsize;

	if ( nelms > val->getNelms() )
		nelms = val->getNelms();


	if        ( view.itemsize == sizeof(uint8_t ) ) {
		uint8_t *bufp = reinterpret_cast<uint8_t*>(view.buf);
		// set same value to all elements ? 
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint16_t) ) {
		uint16_t *bufp = reinterpret_cast<uint16_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint32_t) ) {
		uint32_t *bufp = reinterpret_cast<uint32_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint64_t) ) {
		uint64_t *bufp = reinterpret_cast<uint64_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	}

	throw InvalidArgError("Unable to convert python argument");
}



static boost::python::object wrap_ScalVal_RO_getVal(ScalVal_RO val, int from, int to, bool forceNumeric)
{
Enum       enm   = val->getEnum();
unsigned   nelms = val->getNelms();
unsigned   got;
IndexRange rng(from, to);

	if ( enm && ! forceNumeric ) {

	std::vector<CString>  str;

		str.reserve(nelms);
		got = val->getVal( &str[0], nelms, &rng );
		if ( 1 == got ) {
			return boost::python::object( *str[0] );	
		}

		boost::python::list l;
		for ( unsigned i = 0; i<got; i++ ) {
			l.append( *str[i] );
		}
		return l;

	} else {

	std::vector<uint64_t> v64;

		v64.reserve(nelms);

		got = val->getVal( &v64[0], nelms, &rng );
		if ( 1 == got ) {
			return boost::python::object( v64[0] );
		}

		boost::python::list l;
		for ( unsigned i = 0; i<got; i++ ) {
			l.append( v64[i] );
		}
		return l;
	}
}

static unsigned wrap_ScalVal_setVal(ScalVal val, object &o, int from, int to)
{
PyObject  *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
Py_buffer  view;
IndexRange rng(from, to);

	if (    PyObject_CheckBuffer( op )
	     && 0 == PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS ) ) {
		// new style buffer interface
		ViewGuard guard( &view );

		Py_ssize_t nelms = view.len / view.itemsize;

		if        ( view.itemsize == sizeof(uint8_t ) ) {
			uint8_t *bufp = reinterpret_cast<uint8_t*>(view.buf);
			// set same value to all elements ? 
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint16_t) ) {
			uint16_t *bufp = reinterpret_cast<uint16_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint32_t) ) {
			uint32_t *bufp = reinterpret_cast<uint32_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint64_t) ) {
			uint64_t *bufp = reinterpret_cast<uint64_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		}

		// if we get here then we couldn't deal with the buffer interface;
		// try the hard way...
	}

	// if the object is not a python sequence then 'len()' cannot be used.
	// Thus, we have to handle this case separately...
	if ( ! PySequence_Check( op ) ) {
		if ( val->getEnum() && PyBytes_Check( op ) ) {
			// if we have enums and they give us a string then
			// we supply the string and let CPSW do the mapping
			const char *str_p = extract<const char*>( o );
			return val->setVal( &str_p, 1, &rng );
		} else {
			return val->setVal( extract<uint64_t>( o ), &rng );
		}
	}

	unsigned nelms = len(o);

	if ( val->getEnum() && PyBytes_Check( op ) ) {
		std::vector<const char *> vstr;
		for ( unsigned i = 0; i < nelms; ++i ) {
			vstr.push_back( extract<const char*>( o[i] ) );
		}
		return val->setVal( &vstr[0], nelms, &rng );
	} else {
		std::vector<uint64_t> v64;
		for ( unsigned i = 0; i < nelms; ++i ) {
			v64.push_back( extract<uint64_t>( o[i] ) );
		}
		return val->setVal( &v64[0], nelms, &rng );
	}
}

static int64_t wrap_Stream_read(Stream val, object &o, int64_t timeoutUs)
{
PyObject *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
Py_buffer view;
	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS | PyBUF_WRITEABLE ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	CTimeout timeout;

	if ( timeoutUs >= 0 )
		timeout.set( (uint64_t)timeoutUs );

	return val->read( reinterpret_cast<uint8_t*>(view.buf), view.len, timeout );
}

static int64_t wrap_Stream_write(Stream val, object &o, int64_t timeoutUs)
{
PyObject *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
Py_buffer view;
	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	CTimeout timeout;
	if ( timeoutUs >= 0 )
		timeout.set( (uint64_t)timeoutUs );

	return val->write( reinterpret_cast<uint8_t*>(view.buf), view.len, timeout );
}

// wrap IPathVisitor to call back into python (assuming the visitor
// is implemented there, of course)
class WrapPathVisitor : public IPathVisitor {
private:
	PyObject *self_;

public:
	WrapPathVisitor(PyObject *self)
	: self_(self)
	{
		std::cout << "creating WrapPathVisitor\n";
	}

	virtual bool visitPre(ConstPath here)
	{
		return call_method<bool>(self_, "visitPre", here);
	}

	virtual void visitPost(ConstPath here)
	{
		call_method<void>(self_, "visitPost", here);
	}

	virtual ~WrapPathVisitor()
	{
		std::cout << "deleting WrapPathVisitor\n";
	}
};

BOOST_PYTHON_FUNCTION_OVERLOADS(Hub_loadYamlFile_ol,   IHub::loadYamlFile, 1, 3)

static Hub
wrap_Hub_loadYamlStream(const std::string &yaml, const char *root_name = "root", const char *yaml_dir_name = 0)
{
// couls use IHub::loadYamlStream(const char *,...) but that would make a new string
// which we hopefully can avoid:
std::istringstream sstrm( yaml );
	return IHub::loadYamlStream( sstrm, root_name, yaml_dir_name );
}

BOOST_PYTHON_FUNCTION_OVERLOADS(Hub_loadYamlStream_ol, wrap_Hub_loadYamlStream, 1, 3)

BOOST_PYTHON_MODULE(pycpsw)
{

	register_ptr_to_python<Child                          >();
	register_ptr_to_python<Hub                            >();
	register_ptr_to_python<Path                           >();
	register_ptr_to_python<ConstPath                      >();
	register_ptr_to_python<Enum                           >();
	register_ptr_to_python< shared_ptr<IScalVal_Base>     >();
	register_ptr_to_python<ScalVal_RO                     >();
	register_ptr_to_python<ScalVal                        >();
	register_ptr_to_python<Stream                         >();

	register_ptr_to_python< shared_ptr<std::string const> >();

	// wrap 'IEntry' interface
	class_<IEntry, boost::noncopyable> EntryClazz("Entry", no_init);
	EntryClazz
		.def("getName",        &IEntry::getName)
		.def("getSize",        &IEntry::getSize)
		.def("getDescription", &IEntry::getDescription)
		.def("isHub",          &IEntry::isHub)
	;

	// wrap 'IChild' interface
	class_<IChild, bases<IEntry>, boost::noncopyable> ChildClazz("Child", no_init);
	ChildClazz
		.def("getOwner",       &IChild::getOwner)
		.def("getNelms",       &IChild::getNelms)
	;


	// wrap 'IHub' interface
	class_<IHub, bases<IEntry>, boost::noncopyable> HubClazz("Hub", no_init);
	HubClazz
		.def("findByName",     &IHub::findByName)
		.def("getChild",       &IHub::getChild)
		.def("loadYamlFile",   &IHub::loadYamlFile, Hub_loadYamlFile_ol( args("yamlFileName", "rootName", "yamlIncDirName") ))
		.def("loadYaml",       wrap_Hub_loadYamlStream, Hub_loadYamlStream_ol( args("yamlString", "rootName", "yamlIncDirName") ))
		.staticmethod("loadYamlFile")
	;

	// wrap 'IPath' interface
    class_<IPath, boost::noncopyable> PathClazz("Path", no_init);
	PathClazz
		.def("findByName",   &IPath::findByName)
		.def("up",           &IPath::up)
		.def("empty",        &IPath::empty)
		.def("size",         &IPath::size)
		.def("clear",        wrap_Path_clear)
		.def("origin",       &IPath::origin)
		.def("parent",       &IPath::parent)
		.def("tail",         &IPath::tail)
		.def("toString",     &IPath::toString)
		.def("verifyAtTail", &IPath::verifyAtTail)
		.def("append",       &IPath::append)
		.def("explore",      &IPath::explore)
		.def("concat",       &IPath::concat)
		.def("clone",        &IPath::clone)
		.def("getNelms",     &IPath::getNelms)
		.def("getTailFrom",  &IPath::getTailFrom)
		.def("getTailTo",    &IPath::getTailTo)
		.def("loadConfigFromYamlFile", wrap_Path_loadConfigFromYamlFile,
             ( arg("self"), arg("configYamlFilename"), arg("yamlIncDirname")=0 ),
             "\n"
             "Load a configuration file in YAML format and write out into the hardware.\n"
             "\n"
             "'yamlIncDirname' may point to a directory where included YAML files can\n"
             "be found. Defaults to the directory where the YAML file is located."
         )
		.def("loadConfigFromYamlString", wrap_Path_loadConfigFromYamlString,
             ( arg("self"), arg("configYamlString"), arg("yamlIncDirname")=0 ),
             "\n"
             "Load a configuration from a YAML formatted string and write out into the hardware.\n"
             "\n"
             "'yamlIncDirname' may point to a directory where included YAML files can be found.\n"
             "Defaults to the directory where the YAML file is located.\n"
         )
		.def("loadConfigFromYaml", wrap_Path_loadConfigFromYamlString)
		.def("dumpConfigToYaml",   wrap_Path_dumpConfigToYamlFile)
		.def("dumpConfigToYaml",   wrap_Path_dumpConfigToYaml)
		.def("create",       wrap_Path_create)
		.staticmethod("create")
	;

	class_<IPathVisitor, WrapPathVisitor, boost::noncopyable, boost::shared_ptr<WrapPathVisitor> > WrapPathVisitorClazz("PathVisitor");

	// wrap 'IEnum' interface
	class_<IEnum, boost::noncopyable> Enum_Clazz(
		"Enum",
		"\n"
		"An Enum object is a dictionary with associates strings to numerical\n"
		"values.",
		no_init
	);

	Enum_Clazz
		.def("getNelms",     &IEnum::getNelms,
			"\n"
			"Return the number of entries in this Enum."
		)
		.def("getItems",     wrap_Enum_getItems,
			"\n"
			"Return this enum converted into a python dictionary."
		)
	;

	// wrap 'IScalVal_Base' interface
	class_<IScalVal_Base, bases<IEntry>, boost::noncopyable> ScalVal_BaseClazz(
		"ScalVal_Base",
		"\n"
		"Base class for ScalVal variants.\n",
		no_init
	);

	ScalVal_BaseClazz
		.def("getNelms",     &IScalVal_Base::getNelms,
			"\n"
			"Return number of elements addressed by this ScalVal.\n"
			"\n"
			"The Path used to instantiate a ScalVal may address an array\n"
			"of scalar values. This method returns the number of array elements"
		)
		.def("getSizeBits",  &IScalVal_Base::getSizeBits,
			"\n"
			"Return the size in bits of this ScalVal.\n"
			"\n"
			"If the ScalVal represents an array then the return value is the size\n"
			"of each individual element."
		)
		.def("isSigned",     &IScalVal_Base::isSigned,
			"\n"
			"Return True if this ScalVal represents a signed number.\n"
			"\n"
			"If the ScalVal is read into a wider number than its native bitSize\n"
			"then automatic sign-extension is performed (for signed ScalVals)."
		)
		.def("getPath",      &IScalVal_Base::getPath,
			"\n"
			"Return a copy of the Path which was used to create this ScalVal."
		)
		.def("getEnum",      &IScalVal_Base::getEnum,
			"\n"
			"Return 'Enum' object associated with this ScalVal (if any).\n"
			"\n"
			"An Enum object is a dictionary with associates strings to numerical\n"
			"values."
		)
	;
	// wrap 'IScalVal_RO' interface
	class_<IScalVal_RO, bases<IScalVal_Base>, boost::noncopyable> ScalVal_ROClazz(
		"ScalVal_RO",
		"\n"
		"Read-Only interface for endpoints which support scalar values.\n"
		"\n"
		"This interface supports reading integer values e.g., registers\n"
		"or individual bits. It may also feature an associated map of\n"
		"'enum strings'. E.g., a bit with such a map attached could be\n"
		"read as 'True' or 'False'.\n"
		"\n"
		"NOTE: If no write operations are required then it is preferable\n"
		"      to use the ScalVal_RO interface (as opposed to ScalVal)\n"
		"      since the underlying endpoint may be read-only.",
		no_init
	);

	ScalVal_ROClazz
		.def("getVal",       wrap_ScalVal_RO_getVal,
			( arg("self"), arg("fromIdx") = -1, arg("toIdx") = -1, arg("forceNumeric") = false ),
			"\n"
			"Read one or multiple values, return as a scalar or a list.\n"
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the ScalVal_RO was created are retrieved. All\n"
			"values are represented by a 'c-style flattened' list:\n"
			"\n"
			"  /some/dev[0-1]/item[0-1]\n"
			"\n"
			"would return [ dev0_item0_value, dev0_item1_value, dev1_item0_value, ... ].\n"
			"\n"
			"The indices may be used to only cover the subset of the addressed items starting\n"
			"at 'fromIdx' up to and including 'toIdx'.\n"
			"E.g., if a ScalVal_RO created from the above path is read with:\n"
			"\n"
			"  ScalVal_RO.create( root.findByName('/some/dev[0-1]/item[0-1]') )->getVal( 1, 2 )\n"
			"\n"
			"then [ dev0_item1, dev1_item0 ] would be returned. If both 'fromIdx' and 'toIdx'\n"
			"are negative then all elements are included. A negative 'toIdx' is equivalent to\n"
			"'toIdx' == 'fromIdx' and results in only the single element at 'fromIdx' to be read.\n"
			"\n"
			"If the ScalVal_RO supports an 'enum' menu then the numerical values read from the\n"
			"underlying hardware are mapped back to strings which are returned by 'getVal()'.\n"
			"The optional 'forceNumeric' argument, when set to 'True', suppresses this\n"
			"conversion and fetches the raw numerical values."
		)
		.def(			"getVal",       wrap_ScalVal_RO_getVal_into,
			( arg(			"self"), arg("bufObject"), arg("fromIdx") = -1, arg("toIdx") = -1),
			"\n"
			"Read one or multiple values into a buffer, return the number of items read.\n"
			"\n"
			"This variant of 'getVal()' reads values directly into 'bufObject' which must\n"
			"support the ('new-style') buffer protocol (e.g., numpy.ndarray).\n"
			"If the buffer is larger than the number of elements retrieved (i.e., the\n"
			"return value of this method) then the excess elements are undefined.\n"
			"\n"
			"The index limits ('fromIdx/toIdx') may be used to restrict the number of\n"
			"elements read as described above.\n"
			"\n"
			"No mapping to enum strings is supported by this variant of 'getVal()'."
		)
		.def(			"create",       &IScalVal_RO::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'ScalVal_RO' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod(			"create")
	;

	// wrap 'IScalVal' interface
	class_<IScalVal, bases<IScalVal_RO>, boost::noncopyable> ScalVal_Clazz(
		"ScalVal",
		"\n"
		"Interface for endpoints which support scalar values.\n"
		"\n"
		"This interface supports reading/writing integer values e.g., registers\n"
		"or individual bits. It may also feature an associated map of\n"
		"'enum strings'. E.g., a bit with such a map attached could be\n"
		"read/written as 'True' or 'False'.",
		no_init
	);

	ScalVal_Clazz
		.def(			"setVal",       wrap_ScalVal_setVal,
		    ( arg(			"self"), arg("values"), arg("fromIdx") = -1, arg("toIdx") = -1 ),
			"\n"
			"Write one or multiple values, return the number of elements written."
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the ScalVal was created are written. All values\n"
			"represented by a 'c-style flattened' list or array:\n"
			"\n"
			"  /some/dev[0-1]/item[0-1]\n"
			"\n"
			"would expect [ dev0_item0_value, dev0_item1_value, dev1_item0_value, ... ].\n"
			"\n"
			"The indices may be used to only cover the subset of the addressed items starting\n"
			"at 'fromIdx' up to and including 'toIdx'.\n"
			"E.g., if a ScalVal created from the above path is written with:\n"
			"\n"
			"  ScalVal.create( root.findByName('/some/dev[0-1]/item[0-1]') )->setVal( [44,55], 1, 2 )\n"
			"\n"
			"then dev[0]/item[1] would be written with 44 and dev[1]/item[0] with 55.\n"
			"If both 'fromIdx' and 'toIdx' are negative then all elements are included. A\n"
			"negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only\n"
			"the single element at 'fromIdx' to be written.\n"
			"\n"
			"If the ScalVal has an associated enum 'menu' and 'values' are strings then\n"
			"these strings are mapped by the enum to raw numerical values.\n"
			"\n"
			"If the 'values' object implements the (new-style) buffer protocol then 'setVal()'\n"
			"will extract values directly from the buffer. No enum strings are supported in\n"
			"this case."
		)
		.def("create",       &IScalVal::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'ScalVal' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;

	// wrap 'IStream' interface
	class_<IStream, boost::noncopyable> Stream_Clazz(
		"Stream",
		"\n"
		"Interface for endpoints with support streaming of raw data.",
		no_init
	);

	Stream_Clazz
		.def("read",         wrap_Stream_read,
            ( arg("self"), arg("bufObject"), arg("timeoutUs") = -1 ),
			"\n"
			"Read raw bytes from a streaming interface into a buffer and return the number of bytes read.\n"
			"\n"
			"'bufObject' must support the (new-style) buffer protocol.\n"
			"\n"
			"The 'timeoutUs' argument may be used to limit the time this\n"
			"method blocks waiting for data to arrive. A (relative) timeout\n"
			"in micro-seconds may be specified. A negative timeout blocks\n"
			"indefinitely."
		)
		.def("write",        wrap_Stream_write,
			( arg("self"), arg("bufObject"), arg("timeoutUs") = 0 ),
			"\n"
			"Write raw bytes from a streaming interface into a buffer and return the number of bytes written.\n"
			"\n"
			"'bufObject' must support the (new-style) buffer protocol.\n"
			"\n"
			"The 'timeoutUs' argument may be used to limit the time this\n"
			"method blocks waiting for data to be accepted. A (relative) timeout\n"
			"in micro-seconds may be specified. A negative timeout blocks\n"
			"indefinitely."
		)
		.def("create",       &IStream::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'Stream' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;

	// wrap 'ICommand' interface
	class_<ICommand, bases<IEntry>, boost::noncopyable> Command_Clazz(
		"Command",
		"\n"
		"The Command interface gives access to commands implemented by the underlying endpoint.\n"
		"\n"
		"Details of the command are hidden. Execution runs the command or command sequence\n"
		"coded by the endpoint.",
		no_init
	);

	Command_Clazz
		.def("execute",      &ICommand::execute,
			"\n"
			"Execute the command implemented by the endpoint addressed by the\n"
			"path which was created when instantiating the Command interface."
		)
		.def("create",       &ICommand::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'Stream' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;

	// these macros must all be executed from the same scope so
	// that the 'translator' objects of base classes are still
	// 'alive'.

	ExceptionTranslatorInstall(CPSWError);
	ExceptionTranslatorInstallDerived(DuplicateNameError,           CPSWError);
	ExceptionTranslatorInstallDerived(NotDevError,                  CPSWError);
	ExceptionTranslatorInstallDerived(NotFoundError,                CPSWError);
	ExceptionTranslatorInstallDerived(InvalidPathError,             CPSWError);
	ExceptionTranslatorInstallDerived(InvalidIdentError,            CPSWError);
	ExceptionTranslatorInstallDerived(InvalidArgError,              CPSWError);
	ExceptionTranslatorInstallDerived(AddressAlreadyAttachedError,  CPSWError);
	ExceptionTranslatorInstallDerived(ConfigurationError,           CPSWError);
	ExceptionTranslatorInstallDerived(ErrnoError,                   CPSWError);
	ExceptionTranslatorInstallDerived(InternalError,                ErrnoError);
	ExceptionTranslatorInstallDerived(AddrOutOfRangeError,          CPSWError);
	ExceptionTranslatorInstallDerived(ConversionError,              CPSWError);
	ExceptionTranslatorInstallDerived(InterfaceNotImplementedError, CPSWError);
	ExceptionTranslatorInstallDerived(IOError,                      ErrnoError);
	ExceptionTranslatorInstallDerived(BadStatusError,               CPSWError);
	ExceptionTranslatorInstallDerived(IntrError,                    CPSWError);
	ExceptionTranslatorInstallDerived(StreamDoneError,              CPSWError);
	ExceptionTranslatorInstallDerived(FailedStreamError,            CPSWError);
	ExceptionTranslatorInstallDerived(MissingOnceTagError,          CPSWError);
	ExceptionTranslatorInstallDerived(MissingIncludeFileNameError,  CPSWError);
	ExceptionTranslatorInstallDerived(NoYAMLSupportError,           CPSWError);

}

}
