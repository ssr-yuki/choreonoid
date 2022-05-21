#ifndef CNOID_BODY_PLUGIN_KINEMATIC_BODY_ITEM_SET_H
#define CNOID_BODY_PLUGIN_KINEMATIC_BODY_ITEM_SET_H

#include "BodyItemKinematicsKit.h"
#include <cnoid/KinematicBodySet>
#include "exportdecl.h"

namespace cnoid {

class BodyItem;

class CNOID_EXPORT KinematicBodyItemSet : public KinematicBodySet
{
public:
    KinematicBodyItemSet();

    void setBodyItemPart(int index, BodyItemKinematicsKit* kinematicsKit) {
        KinematicBodySet::setBodyPart(index, kinematicsKit);
    }
    virtual void setBodyPart(int index, BodyKinematicsKit* kinematicsKit) override;

    BodyItemKinematicsKit* bodyItemPart(int index){
        return static_cast<BodyItemKinematicsKit*>(bodyPart(index));
    }
    const BodyItemKinematicsKit* bodyItemPart(int index) const {
        return static_cast<const BodyItemKinematicsKit*>(bodyPart(index));
    }
    BodyItem* bodyItem(int index) {
        if(auto part = bodyItemPart(index)){
            return part->bodyItem();
        }
        return nullptr;
    }
    const BodyItem* bodyItem(int index) const {
        return const_cast<KinematicBodyItemSet*>(this)->bodyItem(index);
    }
    BodyItemKinematicsKit* mainBodyItemPart() {
        return static_cast<BodyItemKinematicsKit*>(mainBodyPart());
    }
    const BodyItemKinematicsKit* mainBodyItemPart() const {
        return static_cast<const BodyItemKinematicsKit*>(mainBodyPart());
    }
    BodyItem* mainBodyItem() {
        return bodyItem(mainBodyPartIndex());
    }
    const BodyItem* mainBodyItem() const {
        return bodyItem(mainBodyPartIndex());
    }

protected:
    KinematicBodyItemSet(const KinematicBodyItemSet& org, CloneMap* cloneMap);
    virtual Referenced* doClone(CloneMap* cloneMap) const override;
};

typedef ref_ptr<KinematicBodyItemSet> KinematicBodyItemSetPtr;

}

#endif
