#pragma once

#define PYTHON_INTERPRETER_NO_DEBUG
#if defined(_DEBUG) && defined(PYTHON_INTERPRETER_NO_DEBUG)
/* Use debug wrappers with the Python release dll */
# undef _DEBUG
# include <Python.h>
# define _DEBUG
#else
# include <Python.h>
#endif