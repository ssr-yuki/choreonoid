#ifndef CNOID_UTIL_COORDINATE_FRAME_H
#define CNOID_UTIL_COORDINATE_FRAME_H

#include "GeneralId.h"
#include "ClonableReferenced.h"
#include <cnoid/EigenTypes>
#include <cnoid/Signal>
#include <string>
#include "exportdecl.h"

namespace cnoid {

class CoordinateFrameList;
class Mapping;

class CNOID_EXPORT CoordinateFrame : public ClonableReferenced
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    CoordinateFrame();
    CoordinateFrame(const GeneralId& id);

    /**
       This constructor is used in a special case where the frame is not actually
       contained in the owner, but the frame needs to set the owner formally.
    */
    CoordinateFrame(const GeneralId& id, CoordinateFrameList* owner);

    CoordinateFrame* clone() const { return static_cast<CoordinateFrame*>(doClone(nullptr)); }
    CoordinateFrame* clone(CloneMap& cloneMap) const { return static_cast<CoordinateFrame*>(doClone(&cloneMap)); }

    const GeneralId& id() const { return id_; }
    bool resetId(const GeneralId& id);

    enum Mode { Local, Global };
    //! \return true if the specified mode is successfully set
    bool setMode(int mode);
    int mode() const { return mode_; }
    bool isLocal() const { return mode_ == Local; }
    bool isGlobal() const { return mode_ == Global; }

    const Isometry3& T() const { return T_; }
    const Isometry3& position() const { return T_; }
    void setPosition(const Isometry3& T) { T_ = T; }

    const std::string& note() const { return note_; }
    void setNote(const std::string& note, bool doNotify = false);

    CoordinateFrameList* ownerFrameList() const;

    bool read(const Mapping& archive);
    bool write(Mapping& archive) const;

    enum UpdateFlag {
        IdUpdate = 1 << 0,
        ModeUpdate = 1 << 1,
        NoteUpdate = 1 << 2,
        PositionUpdate = 1 << 3
    };
    SignalProxy<void(int flags)> sigUpdated() { return sigUpdated_; }
    void notifyUpdate(int flags);

protected:
    CoordinateFrame(const CoordinateFrame& org);
    Referenced* doClone(CloneMap* cloneMap) const override;
        
private:
    Isometry3 T_;
    GeneralId id_;
    int mode_;
    std::string note_;
    weak_ref_ptr<CoordinateFrameList> ownerFrameList_;
    Signal<void(int flags)> sigUpdated_;

    friend class CoordinateFrameList;
};

typedef ref_ptr<CoordinateFrame> CoordinateFramePtr;

}

#endif
