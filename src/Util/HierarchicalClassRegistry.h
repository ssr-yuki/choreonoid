#ifndef CNOID_UTIL_HIERARCHICAL_CLASS_REGISTRY_H
#define CNOID_UTIL_HIERARCHICAL_CLASS_REGISTRY_H

#include <typeinfo>
#include "exportdecl.h"

namespace cnoid {

class CNOID_EXPORT HierarchicalClassRegistryBase
{
public:
    HierarchicalClassRegistryBase();
    ~HierarchicalClassRegistryBase();

    int registerClassAsTypeInfo(const std::type_info& type, const std::type_info& superType);
    int superClassId(int classId) const;
    int numRegisteredClasses() const;

protected:
    int getClassId(const std::type_info& type, int unknownClassId = -1) const;

private:
    class Impl;
    Impl* impl;
};

template<class BaseClass>
class HierarchicalClassRegistry : public HierarchicalClassRegistryBase
{
public:
    HierarchicalClassRegistry() {
        registerClassAsTypeInfo(typeid(BaseClass), typeid(BaseClass));
    }
    
    template<class TargetClass, class SuperClass = BaseClass>
    int registerClass() {
        return registerClassAsTypeInfo(typeid(TargetClass), typeid(SuperClass));
    }

    template<class Object>
    int classId(int unknownClassId = -1) const {
        return getClassId(typeid(Object), unknownClassId);
    }

    int classId(const std::type_info& type, int unknownClassId = -1) const {
        return getClassId(type, unknownClassId);
    }

    int classId(const BaseClass* object, int unknownClassId = -1) const {
        return getClassId(typeid(*object), unknownClassId);
    }
};

}

#endif

