#ifndef SBK_PARAMWRAPPER_H
#define SBK_PARAMWRAPPER_H

#define protected public

#include <shiboken.h>

#include <ParameterWrapper.h>

class ParamWrapper : public Param
{
public:
    virtual ~ParamWrapper();
    static void pysideInitQtMetaTypes();
};

#endif // SBK_PARAMWRAPPER_H

