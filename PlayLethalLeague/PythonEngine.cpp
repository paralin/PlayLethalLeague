#include "stdafx.h"
#include "Game.h"
#include "PythonEngine.h"
#include <boost/python.hpp>
#include <Shlwapi.h>

using namespace boost::python;
namespace py = boost::python;

#if PY_MAJOR_VERSION >= 3
#   define INIT_MODULE PyInit_LethalLeague
extern "C" PyObject* INIT_MODULE();
#else
#error !
#   define INIT_MODULE initLethalLeague
extern "C" void INIT_MODULE();
#endif

#define PRINT_PYTHON_ERROR LOG(parse_python_exception());

std::string parse_python_exception() {
	PyObject *type_ptr = NULL, *value_ptr = NULL, *traceback_ptr = NULL;
	// Fetch the exception info from the Python C API
	PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);

	// Fallback error
	std::string ret("Unfetchable Python error");
	// If the fetch got a type pointer, parse the type into the exception string
	if (type_ptr != NULL) {
		py::handle<> h_type(type_ptr);
		py::str type_pstr(h_type);
		// Extract the string from the boost::python object
		py::extract<std::string> e_type_pstr(type_pstr);
		// If a valid string extraction is available, use it 
		//  otherwise use fallback
		if (e_type_pstr.check())
			ret = e_type_pstr();
		else
			ret = "Unknown exception type";
	}
	// Do the same for the exception value (the stringification of the exception)
	if (value_ptr != NULL) {
		py::handle<> h_val(value_ptr);
		py::str a(h_val);
		py::extract<std::string> returned(a);
		if (returned.check())
			ret += ": " + returned();
		else
			ret += std::string(": Unparseable Python error: ");
	}
	// Parse lines from the traceback using the Python traceback module
	if (traceback_ptr != NULL) {
		py::handle<> h_tb(traceback_ptr);
		// Load the traceback module and the format_tb function
		py::object tb(py::import("traceback"));
		py::object fmt_tb(tb.attr("format_tb"));
		// Call format_tb to get a list of traceback strings
		py::object tb_list(fmt_tb(h_tb));
		// Join the traceback strings into a single string
		py::object tb_str(py::str("\n").join(tb_list));
		// Extract the string, check the extraction, and fallback in necessary
		py::extract<std::string> returned(tb_str);
		if (returned.check())
			ret += ": " + returned();
		else
			ret += std::string(": Unparseable Python traceback");
	}
	return ret;
}

PythonEngine::PythonEngine(Game* game, std::string scriptsRoot)
{
	this->game = game;
	this->scriptsRoot = scriptsRoot;
}


void PythonEngine::initializePython()
{
	LOG("Adding LethalLeague module...");
	PyImport_AppendInittab("LethalLeague", INIT_MODULE);
	LOG("Initializing python...");
	Py_Initialize();
	LOG("Initialized python!");
}

PythonEngine::~PythonEngine()
{
}

bool PythonEngine::loadPythonCode()
{
	std::string expectedPath = scriptsRoot + "neural.py";
	if (!PathFileExists(expectedPath.c_str()))
	{
		LOG("Script expected at: " << expectedPath << " but not found.");
		return false;
	}

	try {
		LOG("Setting up globals...");
		boost::python::object main = boost::python::import("__main__");
		global = object(main.attr("__dict__"));
		LOG("Evaluating " << expectedPath << "...");
		boost::python::exec_file(expectedPath.c_str(), global, global);
		LOG("Executed, extracting data...");
		boost::python::object lethalinter;
		if (!global.contains("LethalInterface") || ((lethalinter = global["LethalInterface"]).is_none()))
		{
			LOG("Script executed fine but doesn't contain a class named LethalInterface.");
			LOG("Please add it and try again.");
			return false;
		}

		LOG("Loaded namespace, attempting to init LethalInterface.");
		boost::python::object lethalinterinst = lethalinter(boost::python::ptr(this->game));
		this->interfaceInstance = lethalinterinst;
		LOG("Loaded LethalInterface successfully & instantiated it.");
	}
	catch(...)
	{
		LOG("Caught some error while loading python code.");
		PRINT_PYTHON_ERROR;
		return false;
	}
	return true;
}

void PythonEngine::newMatchStarted()
{
	try {
		auto fcn = interfaceInstance.attr("newMatchStarted");
		fcn();
	} catch(...)
	{
		PRINT_PYTHON_ERROR;
	}
}

void PythonEngine::playOneFrame()
{
	try {
		auto fcn = interfaceInstance.attr("playOneFrame");
		fcn();
	} catch(...)
	{
		PRINT_PYTHON_ERROR;
	}
}