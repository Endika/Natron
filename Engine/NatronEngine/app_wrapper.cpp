
//workaround to access protected functions
#define protected public

// default includes
#include <shiboken.h>
#include <typeresolver.h>
#include <typeinfo>
#include "natronengine_python.h"

#include "app_wrapper.h"

// Extra includes
#include <NodeWrapper.h>
#include <list>


// Native ---------------------------------------------------------

AppWrapper::~AppWrapper()
{
    SbkObject* wrapper = Shiboken::BindingManager::instance().retrieveWrapper(this);
    Shiboken::Object::destroy(wrapper, this);
}

// Target ---------------------------------------------------------

extern "C" {
static PyObject* Sbk_AppFunc_createNode(PyObject* self, PyObject* args, PyObject* kwds)
{
    ::App* cppSelf = 0;
    SBK_UNUSED(cppSelf)
    if (!Shiboken::Object::isValid(self))
        return 0;
    cppSelf = ((::App*)Shiboken::Conversions::cppPointer(SbkNatronEngineTypes[SBK_APP_IDX], (SbkObject*)self));
    PyObject* pyResult = 0;
    int overloadId = -1;
    PythonToCppFunc pythonToCpp[] = { 0, 0, 0 };
    SBK_UNUSED(pythonToCpp)
    int numNamedArgs = (kwds ? PyDict_Size(kwds) : 0);
    int numArgs = PyTuple_GET_SIZE(args);
    PyObject* pyArgs[] = {0, 0, 0};

    // invalid argument lengths
    if (numArgs + numNamedArgs > 3) {
        PyErr_SetString(PyExc_TypeError, "NatronEngine.App.createNode(): too many arguments");
        return 0;
    } else if (numArgs < 1) {
        PyErr_SetString(PyExc_TypeError, "NatronEngine.App.createNode(): not enough arguments");
        return 0;
    }

    if (!PyArg_ParseTuple(args, "|OOO:createNode", &(pyArgs[0]), &(pyArgs[1]), &(pyArgs[2])))
        return 0;


    // Overloaded function decisor
    // 0: createNode(std::string,int,Effect*)const
    if ((pythonToCpp[0] = Shiboken::Conversions::isPythonToCppConvertible(Shiboken::Conversions::PrimitiveTypeConverter<std::string>(), (pyArgs[0])))) {
        if (numArgs == 1) {
            overloadId = 0; // createNode(std::string,int,Effect*)const
        } else if ((pythonToCpp[1] = Shiboken::Conversions::isPythonToCppConvertible(Shiboken::Conversions::PrimitiveTypeConverter<int>(), (pyArgs[1])))) {
            if (numArgs == 2) {
                overloadId = 0; // createNode(std::string,int,Effect*)const
            } else if ((pythonToCpp[2] = Shiboken::Conversions::isPythonToCppPointerConvertible((SbkObjectType*)SbkNatronEngineTypes[SBK_EFFECT_IDX], (pyArgs[2])))) {
                overloadId = 0; // createNode(std::string,int,Effect*)const
            }
        }
    }

    // Function signature not found.
    if (overloadId == -1) goto Sbk_AppFunc_createNode_TypeError;

    // Call function/method
    {
        if (kwds) {
            PyObject* value = PyDict_GetItemString(kwds, "majorVersion");
            if (value && pyArgs[1]) {
                PyErr_SetString(PyExc_TypeError, "NatronEngine.App.createNode(): got multiple values for keyword argument 'majorVersion'.");
                return 0;
            } else if (value) {
                pyArgs[1] = value;
                if (!(pythonToCpp[1] = Shiboken::Conversions::isPythonToCppConvertible(Shiboken::Conversions::PrimitiveTypeConverter<int>(), (pyArgs[1]))))
                    goto Sbk_AppFunc_createNode_TypeError;
            }
            value = PyDict_GetItemString(kwds, "group");
            if (value && pyArgs[2]) {
                PyErr_SetString(PyExc_TypeError, "NatronEngine.App.createNode(): got multiple values for keyword argument 'group'.");
                return 0;
            } else if (value) {
                pyArgs[2] = value;
                if (!(pythonToCpp[2] = Shiboken::Conversions::isPythonToCppPointerConvertible((SbkObjectType*)SbkNatronEngineTypes[SBK_EFFECT_IDX], (pyArgs[2]))))
                    goto Sbk_AppFunc_createNode_TypeError;
            }
        }
        ::std::string cppArg0;
        pythonToCpp[0](pyArgs[0], &cppArg0);
        int cppArg1 = -1;
        if (pythonToCpp[1]) pythonToCpp[1](pyArgs[1], &cppArg1);
        if (!Shiboken::Object::isValid(pyArgs[2]))
            return 0;
        ::Effect* cppArg2 = 0;
        if (pythonToCpp[2]) pythonToCpp[2](pyArgs[2], &cppArg2);

        if (!PyErr_Occurred()) {
            // createNode(std::string,int,Effect*)const
            // Begin code injection

            Effect * cppResult = cppSelf->createNode(cppArg0,cppArg1,cppArg2);
            pyResult = Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkNatronEngineTypes[SBK_EFFECT_IDX], cppResult);

            // End of code injection



            // Ownership transferences.
            Shiboken::Object::getOwnership(pyResult);
        }
    }

    if (PyErr_Occurred() || !pyResult) {
        Py_XDECREF(pyResult);
        return 0;
    }
    return pyResult;

    Sbk_AppFunc_createNode_TypeError:
        const char* overloads[] = {"std::string, int = -1, NatronEngine.Effect = None", 0};
        Shiboken::setErrorAboutWrongArguments(args, "NatronEngine.App.createNode", overloads);
        return 0;
}

static PyObject* Sbk_AppFunc_getAppID(PyObject* self)
{
    ::App* cppSelf = 0;
    SBK_UNUSED(cppSelf)
    if (!Shiboken::Object::isValid(self))
        return 0;
    cppSelf = ((::App*)Shiboken::Conversions::cppPointer(SbkNatronEngineTypes[SBK_APP_IDX], (SbkObject*)self));
    PyObject* pyResult = 0;

    // Call function/method
    {

        if (!PyErr_Occurred()) {
            // getAppID()const
            PyThreadState* _save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
            int cppResult = const_cast<const ::App*>(cppSelf)->getAppID();
            PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
            pyResult = Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<int>(), &cppResult);
        }
    }

    if (PyErr_Occurred() || !pyResult) {
        Py_XDECREF(pyResult);
        return 0;
    }
    return pyResult;
}

static PyObject* Sbk_AppFunc_timelineGetLeftBound(PyObject* self)
{
    ::App* cppSelf = 0;
    SBK_UNUSED(cppSelf)
    if (!Shiboken::Object::isValid(self))
        return 0;
    cppSelf = ((::App*)Shiboken::Conversions::cppPointer(SbkNatronEngineTypes[SBK_APP_IDX], (SbkObject*)self));
    PyObject* pyResult = 0;

    // Call function/method
    {

        if (!PyErr_Occurred()) {
            // timelineGetLeftBound()const
            PyThreadState* _save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
            int cppResult = const_cast<const ::App*>(cppSelf)->timelineGetLeftBound();
            PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
            pyResult = Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<int>(), &cppResult);
        }
    }

    if (PyErr_Occurred() || !pyResult) {
        Py_XDECREF(pyResult);
        return 0;
    }
    return pyResult;
}

static PyObject* Sbk_AppFunc_timelineGetRightBound(PyObject* self)
{
    ::App* cppSelf = 0;
    SBK_UNUSED(cppSelf)
    if (!Shiboken::Object::isValid(self))
        return 0;
    cppSelf = ((::App*)Shiboken::Conversions::cppPointer(SbkNatronEngineTypes[SBK_APP_IDX], (SbkObject*)self));
    PyObject* pyResult = 0;

    // Call function/method
    {

        if (!PyErr_Occurred()) {
            // timelineGetRightBound()const
            PyThreadState* _save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
            int cppResult = const_cast<const ::App*>(cppSelf)->timelineGetRightBound();
            PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
            pyResult = Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<int>(), &cppResult);
        }
    }

    if (PyErr_Occurred() || !pyResult) {
        Py_XDECREF(pyResult);
        return 0;
    }
    return pyResult;
}

static PyObject* Sbk_AppFunc_timelineGetTime(PyObject* self)
{
    ::App* cppSelf = 0;
    SBK_UNUSED(cppSelf)
    if (!Shiboken::Object::isValid(self))
        return 0;
    cppSelf = ((::App*)Shiboken::Conversions::cppPointer(SbkNatronEngineTypes[SBK_APP_IDX], (SbkObject*)self));
    PyObject* pyResult = 0;

    // Call function/method
    {

        if (!PyErr_Occurred()) {
            // timelineGetTime()const
            PyThreadState* _save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
            int cppResult = const_cast<const ::App*>(cppSelf)->timelineGetTime();
            PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
            pyResult = Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<int>(), &cppResult);
        }
    }

    if (PyErr_Occurred() || !pyResult) {
        Py_XDECREF(pyResult);
        return 0;
    }
    return pyResult;
}

static PyMethodDef Sbk_App_methods[] = {
    {"createNode", (PyCFunction)Sbk_AppFunc_createNode, METH_VARARGS|METH_KEYWORDS},
    {"getAppID", (PyCFunction)Sbk_AppFunc_getAppID, METH_NOARGS},
    {"timelineGetLeftBound", (PyCFunction)Sbk_AppFunc_timelineGetLeftBound, METH_NOARGS},
    {"timelineGetRightBound", (PyCFunction)Sbk_AppFunc_timelineGetRightBound, METH_NOARGS},
    {"timelineGetTime", (PyCFunction)Sbk_AppFunc_timelineGetTime, METH_NOARGS},

    {0} // Sentinel
};

} // extern "C"

static int Sbk_App_traverse(PyObject* self, visitproc visit, void* arg)
{
    return reinterpret_cast<PyTypeObject*>(&SbkObject_Type)->tp_traverse(self, visit, arg);
}
static int Sbk_App_clear(PyObject* self)
{
    return reinterpret_cast<PyTypeObject*>(&SbkObject_Type)->tp_clear(self);
}
// Class Definition -----------------------------------------------
extern "C" {
static SbkObjectType Sbk_App_Type = { { {
    PyVarObject_HEAD_INIT(&SbkObjectType_Type, 0)
    /*tp_name*/             "NatronEngine.App",
    /*tp_basicsize*/        sizeof(SbkObject),
    /*tp_itemsize*/         0,
    /*tp_dealloc*/          &SbkDeallocWrapper,
    /*tp_print*/            0,
    /*tp_getattr*/          0,
    /*tp_setattr*/          0,
    /*tp_compare*/          0,
    /*tp_repr*/             0,
    /*tp_as_number*/        0,
    /*tp_as_sequence*/      0,
    /*tp_as_mapping*/       0,
    /*tp_hash*/             0,
    /*tp_call*/             0,
    /*tp_str*/              0,
    /*tp_getattro*/         0,
    /*tp_setattro*/         0,
    /*tp_as_buffer*/        0,
    /*tp_flags*/            Py_TPFLAGS_DEFAULT|Py_TPFLAGS_CHECKTYPES|Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/              0,
    /*tp_traverse*/         Sbk_App_traverse,
    /*tp_clear*/            Sbk_App_clear,
    /*tp_richcompare*/      0,
    /*tp_weaklistoffset*/   0,
    /*tp_iter*/             0,
    /*tp_iternext*/         0,
    /*tp_methods*/          Sbk_App_methods,
    /*tp_members*/          0,
    /*tp_getset*/           0,
    /*tp_base*/             0,
    /*tp_dict*/             0,
    /*tp_descr_get*/        0,
    /*tp_descr_set*/        0,
    /*tp_dictoffset*/       0,
    /*tp_init*/             0,
    /*tp_alloc*/            0,
    /*tp_new*/              0,
    /*tp_free*/             0,
    /*tp_is_gc*/            0,
    /*tp_bases*/            0,
    /*tp_mro*/              0,
    /*tp_cache*/            0,
    /*tp_subclasses*/       0,
    /*tp_weaklist*/         0
}, },
    /*priv_data*/           0
};
} //extern

static void* Sbk_App_typeDiscovery(void* cptr, SbkObjectType* instanceType)
{
    if (instanceType == reinterpret_cast<SbkObjectType*>(Shiboken::SbkType< ::Group >()))
        return dynamic_cast< ::App*>(reinterpret_cast< ::Group*>(cptr));
    return 0;
}


// Type conversion functions.

// Python to C++ pointer conversion - returns the C++ object of the Python wrapper (keeps object identity).
static void App_PythonToCpp_App_PTR(PyObject* pyIn, void* cppOut) {
    Shiboken::Conversions::pythonToCppPointer(&Sbk_App_Type, pyIn, cppOut);
}
static PythonToCppFunc is_App_PythonToCpp_App_PTR_Convertible(PyObject* pyIn) {
    if (pyIn == Py_None)
        return Shiboken::Conversions::nonePythonToCppNullPtr;
    if (PyObject_TypeCheck(pyIn, (PyTypeObject*)&Sbk_App_Type))
        return App_PythonToCpp_App_PTR;
    return 0;
}

// C++ to Python pointer conversion - tries to find the Python wrapper for the C++ object (keeps object identity).
static PyObject* App_PTR_CppToPython_App(const void* cppIn) {
    PyObject* pyOut = (PyObject*)Shiboken::BindingManager::instance().retrieveWrapper(cppIn);
    if (pyOut) {
        Py_INCREF(pyOut);
        return pyOut;
    }
    const char* typeName = typeid(*((::App*)cppIn)).name();
    return Shiboken::Object::newObject(&Sbk_App_Type, const_cast<void*>(cppIn), false, false, typeName);
}

void init_App(PyObject* module)
{
    SbkNatronEngineTypes[SBK_APP_IDX] = reinterpret_cast<PyTypeObject*>(&Sbk_App_Type);

    if (!Shiboken::ObjectType::introduceWrapperType(module, "App", "App*",
        &Sbk_App_Type, &Shiboken::callCppDestructor< ::App >, (SbkObjectType*)SbkNatronEngineTypes[SBK_GROUP_IDX])) {
        return;
    }

    // Register Converter
    SbkConverter* converter = Shiboken::Conversions::createConverter(&Sbk_App_Type,
        App_PythonToCpp_App_PTR,
        is_App_PythonToCpp_App_PTR_Convertible,
        App_PTR_CppToPython_App);

    Shiboken::Conversions::registerConverterName(converter, "App");
    Shiboken::Conversions::registerConverterName(converter, "App*");
    Shiboken::Conversions::registerConverterName(converter, "App&");
    Shiboken::Conversions::registerConverterName(converter, typeid(::App).name());
    Shiboken::Conversions::registerConverterName(converter, typeid(::AppWrapper).name());


    Shiboken::ObjectType::setTypeDiscoveryFunctionV2(&Sbk_App_Type, &Sbk_App_typeDiscovery);


}