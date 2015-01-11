

#ifndef SBK_NATRONENGINE_PYTHON_H
#define SBK_NATRONENGINE_PYTHON_H

//workaround to access protected functions
#define protected public

#include <sbkpython.h>
#include <conversions.h>
#include <sbkenum.h>
#include <basewrapper.h>
#include <bindingmanager.h>
#include <memory>

// Module Includes
#include <pyside_qtcore_python.h>

// Binded library includes
#include <Enums.h>
#include <NodeWrapper.h>
#include <GlobalDefines.h>
#include <NodeGroupWrapper.h>
#include <RotoWrapper.h>
#include <AppInstanceWrapper.h>
#include <ParameterWrapper.h>
// Conversion Includes - Primitive Types
#include <QString>
#include <signalmanager.h>
#include <typeresolver.h>
#include <QtConcurrentFilter>
#include <QStringList>
#include <qabstractitemmodel.h>

// Conversion Includes - Container Types
#include <QList>
#include <QMap>
#include <QStack>
#include <QMultiMap>
#include <map>
#include <QVector>
#include <QPair>
#include <pysideconversions.h>
#include <QSet>
#include <vector>
#include <QQueue>
#include <map>
#include <utility>
#include <list>
#include <QLinkedList>
#include <set>

// Type indices
#define SBK_NATRON_IDX                                               25
#define SBK_NATRON_STANDARDBUTTONENUM_IDX                            34
#define SBK_QFLAGS_NATRON_STANDARDBUTTONENUM__IDX                    44
#define SBK_NATRON_IMAGECOMPONENTSENUM_IDX                           28
#define SBK_NATRON_IMAGEBITDEPTHENUM_IDX                             27
#define SBK_NATRON_KEYFRAMETYPEENUM_IDX                              30
#define SBK_NATRON_VALUECHANGEDREASONENUM_IDX                        36
#define SBK_NATRON_ANIMATIONLEVELENUM_IDX                            26
#define SBK_NATRON_ORIENTATIONENUM_IDX                               31
#define SBK_NATRON_IMAGEPREMULTIPLICATIONENUM_IDX                    29
#define SBK_NATRON_STATUSENUM_IDX                                    35
#define SBK_NATRON_VIEWERCOMPOSITINGOPERATORENUM_IDX                 38
#define SBK_NATRON_PLAYBACKMODEENUM_IDX                              33
#define SBK_NATRON_PIXMAPENUM_IDX                                    32
#define SBK_NATRON_VIEWERCOLORSPACEENUM_IDX                          37
#define SBK_COLORTUPLE_IDX                                           8
#define SBK_DOUBLE3DTUPLE_IDX                                        12
#define SBK_DOUBLE2DTUPLE_IDX                                        10
#define SBK_INT3DTUPLE_IDX                                           21
#define SBK_INT2DTUPLE_IDX                                           19
#define SBK_PARAM_IDX                                                41
#define SBK_ANIMATEDPARAM_IDX                                        0
#define SBK_STRINGPARAMBASE_IDX                                      48
#define SBK_FILEPARAM_IDX                                            15
#define SBK_STRINGPARAM_IDX                                          46
#define SBK_STRINGPARAM_TYPEENUM_IDX                                 47
#define SBK_PATHPARAM_IDX                                            43
#define SBK_OUTPUTFILEPARAM_IDX                                      39
#define SBK_BOOLEANPARAM_IDX                                         4
#define SBK_CHOICEPARAM_IDX                                          6
#define SBK_COLORPARAM_IDX                                           7
#define SBK_DOUBLEPARAM_IDX                                          13
#define SBK_DOUBLE2DPARAM_IDX                                        9
#define SBK_DOUBLE3DPARAM_IDX                                        11
#define SBK_INTPARAM_IDX                                             22
#define SBK_INT2DPARAM_IDX                                           18
#define SBK_INT3DPARAM_IDX                                           20
#define SBK_PARAMETRICPARAM_IDX                                      42
#define SBK_PAGEPARAM_IDX                                            40
#define SBK_GROUPPARAM_IDX                                           17
#define SBK_BUTTONPARAM_IDX                                          5
#define SBK_ROTO_IDX                                                 45
#define SBK_ITEMBASE_IDX                                             23
#define SBK_BEZIERCURVE_IDX                                          2
#define SBK_BEZIERCURVE_CAIROOPERATORENUM_IDX                        3
#define SBK_LAYER_IDX                                                24
#define SBK_GROUP_IDX                                                16
#define SBK_EFFECT_IDX                                               14
#define SBK_APP_IDX                                                  1
#define SBK_NatronEngine_IDX_COUNT                                   49

// This variable stores all Python types exported by this module.
extern PyTypeObject** SbkNatronEngineTypes;

// This variable stores all type converters exported by this module.
extern SbkConverter** SbkNatronEngineTypeConverters;

// Converter indices
#define SBK_STD_SIZE_T_IDX                                           0
#define SBK_NATRONENGINE_STD_LIST_STD_STRING_IDX                     1 // std::list<std::string >
#define SBK_NATRONENGINE_STD_PAIR_STD_STRING_STD_STRING_IDX          2 // std::pair<std::string, std::string >
#define SBK_NATRONENGINE_STD_LIST_STD_PAIR_STD_STRING_STD_STRING_IDX 3 // const std::list<std::pair<std::string, std::string > > &
#define SBK_NATRONENGINE_STD_LIST_ITEMBASEPTR_IDX                    4 // std::list<ItemBase * >
#define SBK_NATRONENGINE_STD_LIST_EFFECTPTR_IDX                      5 // std::list<Effect * >
#define SBK_NATRONENGINE_STD_LIST_PARAMPTR_IDX                       6 // std::list<Param * >
#define SBK_NATRONENGINE_QLIST_QVARIANT_IDX                          7 // QList<QVariant >
#define SBK_NATRONENGINE_QLIST_QSTRING_IDX                           8 // QList<QString >
#define SBK_NATRONENGINE_QMAP_QSTRING_QVARIANT_IDX                   9 // QMap<QString, QVariant >
#define SBK_NatronEngine_CONVERTERS_IDX_COUNT                        10

// Macros for type check

namespace Shiboken
{

// PyType functions, to get the PyObjectType for a type T
template<> inline PyTypeObject* SbkType< ::Natron::StandardButtonEnum >() { return SbkNatronEngineTypes[SBK_NATRON_STANDARDBUTTONENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::QFlags<Natron::StandardButtonEnum> >() { return SbkNatronEngineTypes[SBK_QFLAGS_NATRON_STANDARDBUTTONENUM__IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ImageComponentsEnum >() { return SbkNatronEngineTypes[SBK_NATRON_IMAGECOMPONENTSENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ImageBitDepthEnum >() { return SbkNatronEngineTypes[SBK_NATRON_IMAGEBITDEPTHENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::KeyframeTypeEnum >() { return SbkNatronEngineTypes[SBK_NATRON_KEYFRAMETYPEENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ValueChangedReasonEnum >() { return SbkNatronEngineTypes[SBK_NATRON_VALUECHANGEDREASONENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::AnimationLevelEnum >() { return SbkNatronEngineTypes[SBK_NATRON_ANIMATIONLEVELENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::OrientationEnum >() { return SbkNatronEngineTypes[SBK_NATRON_ORIENTATIONENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ImagePremultiplicationEnum >() { return SbkNatronEngineTypes[SBK_NATRON_IMAGEPREMULTIPLICATIONENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::StatusEnum >() { return SbkNatronEngineTypes[SBK_NATRON_STATUSENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ViewerCompositingOperatorEnum >() { return SbkNatronEngineTypes[SBK_NATRON_VIEWERCOMPOSITINGOPERATORENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::PlaybackModeEnum >() { return SbkNatronEngineTypes[SBK_NATRON_PLAYBACKMODEENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::PixmapEnum >() { return SbkNatronEngineTypes[SBK_NATRON_PIXMAPENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::Natron::ViewerColorSpaceEnum >() { return SbkNatronEngineTypes[SBK_NATRON_VIEWERCOLORSPACEENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::ColorTuple >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_COLORTUPLE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Double3DTuple >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_DOUBLE3DTUPLE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Double2DTuple >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_DOUBLE2DTUPLE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Int3DTuple >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_INT3DTUPLE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Int2DTuple >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_INT2DTUPLE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Param >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_PARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::AnimatedParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_ANIMATEDPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::StringParamBase >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_STRINGPARAMBASE_IDX]); }
template<> inline PyTypeObject* SbkType< ::FileParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_FILEPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::StringParam::TypeEnum >() { return SbkNatronEngineTypes[SBK_STRINGPARAM_TYPEENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::StringParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_STRINGPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::PathParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_PATHPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::OutputFileParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_OUTPUTFILEPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::BooleanParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_BOOLEANPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::ChoiceParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_CHOICEPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::ColorParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_COLORPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::DoubleParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_DOUBLEPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::Double2DParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_DOUBLE2DPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::Double3DParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_DOUBLE3DPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::IntParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_INTPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::Int2DParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_INT2DPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::Int3DParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_INT3DPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::ParametricParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_PARAMETRICPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::PageParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_PAGEPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::GroupParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_GROUPPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::ButtonParam >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_BUTTONPARAM_IDX]); }
template<> inline PyTypeObject* SbkType< ::Roto >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_ROTO_IDX]); }
template<> inline PyTypeObject* SbkType< ::ItemBase >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_ITEMBASE_IDX]); }
template<> inline PyTypeObject* SbkType< ::BezierCurve::CairoOperatorEnum >() { return SbkNatronEngineTypes[SBK_BEZIERCURVE_CAIROOPERATORENUM_IDX]; }
template<> inline PyTypeObject* SbkType< ::BezierCurve >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_BEZIERCURVE_IDX]); }
template<> inline PyTypeObject* SbkType< ::Layer >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_LAYER_IDX]); }
template<> inline PyTypeObject* SbkType< ::Group >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_GROUP_IDX]); }
template<> inline PyTypeObject* SbkType< ::Effect >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_EFFECT_IDX]); }
template<> inline PyTypeObject* SbkType< ::App >() { return reinterpret_cast<PyTypeObject*>(SbkNatronEngineTypes[SBK_APP_IDX]); }

} // namespace Shiboken

#endif // SBK_NATRONENGINE_PYTHON_H
