/** \file
    \author Shin'ichiro Nakaoka
*/

#include "LinkPositionView.h"
#include "BodySelectionManager.h"
#include <cnoid/PositionWidget>
#include <cnoid/BodyItem>
#include <cnoid/Body>
#include <cnoid/Link>
#include <cnoid/JointPath>
#include <cnoid/JointSpaceConfigurationHandler>
#include <cnoid/CompositeBodyIK>
#include <cnoid/LinkCoordFrameSetSuite>
#include <cnoid/LinkKinematicsKit>
#include <cnoid/BodyState>
#include <cnoid/EigenUtil>
#include <cnoid/ConnectionSet>
#include <cnoid/ViewManager>
#include <cnoid/MenuManager>
#include <cnoid/PositionEditManager>
#include <cnoid/Archive>
#include <cnoid/Buttons>
#include <cnoid/CheckBox>
#include <cnoid/ComboBox>
#include <cnoid/ButtonGroup>
#include <cnoid/ActionGroup>
#include <cnoid/Dialog>
#include <cnoid/TreeWidget>
#include <cnoid/LineEdit>
#include <cnoid/Selection>
#include <QScrollArea>
#include <QLabel>
#include <QGridLayout>
#include <QStyle>
#include <QDialogButtonBox>
#include <fmt/format.h>
#include <unordered_set>
#include "gettext.h"

using namespace std;
using namespace cnoid;
using fmt::format;

namespace {

const QString normalStyle("font-weight: normal");
const QString errorStyle("font-weight: bold; color: red");

enum FrameType {
    WorldFrame = LinkKinematicsKit::WorldFrame,
    BodyFrame = LinkKinematicsKit::BodyFrame,
    LinkFrame = LinkKinematicsKit::LinkFrame
};

enum FrameComboType { BaseFrameCombo, LinkFrameCombo };

class ConfTreeWidget : public TreeWidget
{
public:
    /*
    QModelIndex indexFromItem(const QTreeWidgetItem* item, int column = 0) const {
        return QTreeWidget::indexFromItem(item, column);
    }
    */
    virtual QSize sizeHint() const override;
};

class JointSpaceConfigurationDialog : public Dialog
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    LinkPositionView::Impl* view;
    BodyPtr body;
    shared_ptr<JointPath> jointPath;
    shared_ptr<JointSpaceConfigurationHandler> configuration;
    Position T0;
    BodyState bodyState0;
    ConfTreeWidget treeWidget;
    LineEdit searchBox;
    CheckBox feasibleCheck;
    int lastSortedSection;
    Qt::SortOrder lastSortOrder;
    QRect lastPosition;

    JointSpaceConfigurationDialog(LinkPositionView::Impl* view);
    void reset();
    bool updateConfigurationTypes();
    void updateConfigurationStates();
    void updateItemDisplay();
    void onSectionClicked(int index);
    void applyConfiguration(int id);
    void onCanceled();
    virtual void hideEvent(QHideEvent* event) override;
};

}

namespace cnoid {

class LinkPositionView::Impl
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    LinkPositionView* self;

    ScopedConnectionSet activeStateConnections;
    ScopedConnectionSet targetConnections;
    enum TargetType { LinkTarget, PositionEditTarget } targetType;
    BodyItemPtr targetBodyItem;
    LinkPtr targetLink;
    enum TargetLinkType { AnyLink, RootOrIkLink, IkLink, NumTargetLinkTypes };
    Selection targetLinkTypeSelection;
    LinkKinematicsKitPtr kinematicsKit;
    ScopedConnection kinematicsKitConnection;
    CoordinateFramePtr identityFrame;
    CoordinateFramePtr baseFrame;
    CoordinateFramePtr linkFrame;
    std::function<std::tuple<std::string,std::string,std::string>(LinkKinematicsKit*)> functionToGetDefaultFrameNames;
    AbstractPositionEditTarget* positionEditTarget;
    
    QLabel targetLabel;
    QLabel resultLabel;

    enum CoordinateMode { WorldCoordinateMode, BodyCoordinateMode, LocalCoordinateMode, NumCoordinateModes };
    Selection coordinateModeSelection;
    int coordinateMode;
    int preferredCoordinateMode;
    ButtonGroup coordinateModeGroup;
    RadioButton worldCoordRadio;
    RadioButton bodyCoordRadio;
    RadioButton localCoordRadio;
    vector<QWidget*> coordinateModeWidgets;

    PositionWidget* positionWidget;

    string defaultCoordName[3];
    QLabel frameComboLabel[2];
    ComboBox frameCombo[2];
    
    QLabel configurationLabel;
    vector<int> currentConfigurationTypes;
    string configurationString;
    std::unordered_set<string> tmpConfigurationLabelSet;
    ToolButton configurationButton;
    vector<QWidget*> configurationWidgets;
    JointSpaceConfigurationDialog* configurationDialog;
    
    ScopedConnectionSet userInputConnections;

    Impl(LinkPositionView* self);
    void createPanel();
    void onActivated();
    void onAttachedMenuRequest(MenuManager& menuManager);
    void setCoordinateModeInterfaceEnabled(bool on);
    void setCoordinateMode(int mode, bool doUpdateDisplay);
    void setBodyCoordinateModeEnabled(bool on);
    void onCoordinateModeRadioToggled(int mode);
    void setTargetLinkType(int type);
    void setTargetBodyAndLink(BodyItem* bodyItem, Link* link);
    void updateTargetLink(Link* link);
    void setCoordinateFrameInterfaceEnabled(bool on);
    void updateCoordinateFrameCandidates();
    void updateCoordinateFrameCandidates(int frameComboIndex);
    void updateCoordinateFrameComboItems(
        QComboBox& combo, CoordinateFrameSet* frames, const GeneralId& currentId, const std::string& originLabel);
    void onFrameComboActivated(int frameComboIndex, int index);
    void onFrameUpdate();
    void setConfigurationInterfaceEnabled(bool on);
    void initializeConfigurationInterface();
    void showConfigurationDialog();
    void applyConfiguration(int id);
    bool setPositionEditTarget(AbstractPositionEditTarget* target);
    void onPositionEditTargetExpired();
    void updateDisplay();
    void updateDisplayWithCurrentLinkPosition();
    void updateDisplayWithPositionEditTarget();
    void updateConfigurationDisplay();
    void setFramesToCombo(CoordinateFrameSet* frames, QComboBox& combo);
    bool applyPositionInput(const Position& T);
    bool findBodyIkSolution(const Position& T_input, bool isRawT);
    bool applyInputToPositionEditTarget(const Position& T_input);
    bool storeState(Archive& archive);
    bool restoreState(const Archive& archive);
};

}


void LinkPositionView::initializeClass(ExtensionManager* ext)
{
    ext->viewManager().registerClass<LinkPositionView>(
        "LinkPositionView", N_("Link Position"), ViewManager::SINGLE_OPTIONAL);
}


LinkPositionView* LinkPositionView::instance()
{
    static LinkPositionView* instance_ = ViewManager::getOrCreateView<LinkPositionView>();
    return instance_;
}


LinkPositionView::LinkPositionView()
{
    impl = new Impl(this);
}


LinkPositionView::Impl::Impl(LinkPositionView* self)
    : self(self),
      targetLinkTypeSelection(NumTargetLinkTypes, CNOID_GETTEXT_DOMAIN_NAME),
      coordinateModeSelection(NumCoordinateModes, CNOID_GETTEXT_DOMAIN_NAME)
{
    self->setDefaultLayoutArea(View::CENTER);
    createPanel();
    self->setEnabled(false);
    
    targetType = LinkTarget;

    targetLinkTypeSelection.setSymbol(AnyLink, "any_link");    
    targetLinkTypeSelection.setSymbol(RootOrIkLink, "root_or_ik_link");    
    targetLinkTypeSelection.setSymbol(IkLink, "ik_link");    
    targetLinkTypeSelection.select(RootOrIkLink);
    
    identityFrame = new CoordinateFrame;
    baseFrame = identityFrame;
    linkFrame = identityFrame;

    positionEditTarget = nullptr;
}


LinkPositionView::~LinkPositionView()
{
    delete impl;
}


void LinkPositionView::Impl::createPanel()
{
    self->setDefaultLayoutArea(View::CENTER);

    auto baseLayout = new QVBoxLayout;
    self->setLayout(baseLayout);

    auto scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidgetResizable(true);
    baseLayout->addWidget(scrollArea);
    
    auto baseWidget = new QWidget;
    baseWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    scrollArea->setWidget(baseWidget);
    
    auto vbox = new QVBoxLayout;
    self->setLayoutContentsMarginRatio(vbox, 1.0, 0.5, 1.0, 0.0);
    baseWidget->setLayout(vbox);

    auto hbox = new QHBoxLayout;
    hbox->addStretch(2);
    targetLabel.setStyleSheet("font-weight: bold");
    targetLabel.setAlignment(Qt::AlignLeft);
    hbox->addWidget(&targetLabel);
    hbox->addStretch(10);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    resultLabel.setFrameStyle(QFrame::Box | QFrame::Sunken);
    resultLabel.setAlignment(Qt::AlignCenter);
    hbox->addWidget(&resultLabel, 1);
    auto actualButton = new PushButton(_("Fetch"));
    actualButton->sigClicked().connect([&](){ updateDisplay(); });
    hbox->addWidget(actualButton);
    auto applyButton = new PushButton(_("Apply"));
    applyButton->sigClicked().connect([&](){ positionWidget->applyPositionInput(); });
    hbox->addWidget(applyButton);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    auto coordLabel = new QLabel(_("Coord:"));
    hbox->addWidget(coordLabel);
    coordinateModeWidgets.push_back(coordLabel);
    
    coordinateModeSelection.setSymbol(WorldCoordinateMode, "world");
    worldCoordRadio.setText(_("World"));
    worldCoordRadio.setChecked(true);
    hbox->addWidget(&worldCoordRadio);
    coordinateModeGroup.addButton(&worldCoordRadio, WorldCoordinateMode);
    coordinateModeWidgets.push_back(&worldCoordRadio);

    coordinateModeSelection.setSymbol(BodyCoordinateMode, "body");
    bodyCoordRadio.setText(_("Body"));
    hbox->addWidget(&bodyCoordRadio);
    coordinateModeGroup.addButton(&bodyCoordRadio, BodyCoordinateMode);
    coordinateModeWidgets.push_back(&bodyCoordRadio);

    coordinateModeSelection.setSymbol(LocalCoordinateMode, "local");
    localCoordRadio.setText(_("Local"));
    hbox->addWidget(&localCoordRadio);
    coordinateModeGroup.addButton(&localCoordRadio, LocalCoordinateMode);
    coordinateModeWidgets.push_back(&localCoordRadio);
    localCoordRadio.setEnabled(false);

    coordinateMode = WorldCoordinateMode;
    preferredCoordinateMode = BodyCoordinateMode;

    coordinateModeGroup.sigButtonToggled().connect(
        [&](int id, bool checked){
            if(checked) onCoordinateModeRadioToggled(id);
        });
    
    hbox->addStretch();
    vbox->addLayout(hbox);

    positionWidget = new PositionWidget(self);
    positionWidget->setUserInputValuePriorityMode(true);
    positionWidget->setPositionCallback(
        [&](const Position& T){ return applyPositionInput(T); });
    vbox->addWidget(positionWidget);

    auto grid = new QGridLayout;
    int row = 0;
    grid->setColumnStretch(1, 1);

    frameComboLabel[BaseFrameCombo].setText(_("Base"));
    frameComboLabel[LinkFrameCombo].setText(_("End"));

    for(int i=0; i < 2; ++i){
        grid->addWidget(&frameComboLabel[i], row + i, 0, Qt::AlignLeft /* Qt::AlignJustify */);
        
        frameCombo[i].sigAboutToShowPopup().connect(
            [=](){ updateCoordinateFrameCandidates(i); });
        
        frameCombo[i].sigActivated().connect(
            [=](int index){ onFrameComboActivated(i, index); });
        
        grid->addWidget(&frameCombo[i], row + i, 1, 1, 2);
    }
    row += 2;

    auto configTitle = new QLabel(_("Config"));
    grid->addWidget(configTitle, row, 0, Qt::AlignLeft);
    configurationWidgets.push_back(configTitle);
    grid->addWidget(&configurationLabel, row, 1,  Qt::AlignLeft);
    configurationWidgets.push_back(&configurationLabel);
    configurationButton.setText(_("Set"));
    userInputConnections.add(
        configurationButton.sigClicked().connect(
            [this](){ showConfigurationDialog(); }));
    grid->addWidget(&configurationButton, row, 2);
    configurationWidgets.push_back(&configurationButton);
    configurationDialog = nullptr;
    vbox->addLayout(grid);

    vbox->addStretch();
}


void LinkPositionView::setCoordinateModeLabels
(const char* worldModeLabel, const char* baseModeLabel, const char* localModeLabel)
{
    impl->worldCoordRadio.setText(worldModeLabel);
    impl->bodyCoordRadio.setText(baseModeLabel);
    impl->localCoordRadio.setText(localModeLabel);
}


void LinkPositionView::setCoordinateOffsetLabels(const char* baseOffsetLabel, const char* linkOffsetLabel)
{
    impl->frameComboLabel[BaseFrameCombo].setText(baseOffsetLabel);
    impl->frameComboLabel[LinkFrameCombo].setText(linkOffsetLabel);
}


void LinkPositionView::customizeDefaultCoordinateFrameNames
(std::function<std::tuple<std::string,std::string,std::string>(LinkKinematicsKit*)> getNames)
{
    impl->functionToGetDefaultFrameNames = getNames;
}


void LinkPositionView::onActivated()
{
    impl->onActivated();
}


void LinkPositionView::Impl::onActivated()
{
    auto bsm = BodySelectionManager::instance();
    auto pem = PositionEditManager::instance();
    
    activeStateConnections.add(
        bsm->sigCurrentSpecified().connect(
            [&](BodyItem* bodyItem, Link* link){
                setTargetBodyAndLink(bodyItem, link); }));

    activeStateConnections.add(
        pem->sigPositionEditRequest().connect(
            [&](AbstractPositionEditTarget* target){
                return setPositionEditTarget(target); }));
                
    setTargetBodyAndLink(bsm->currentBodyItem(), bsm->currentLink());

    if(!targetBodyItem){
        if(auto positionEditTarget = pem->lastPositionEditTarget()){
            setPositionEditTarget(positionEditTarget);
        }
    }
}


void LinkPositionView::onDeactivated()
{
    impl->activeStateConnections.disconnect();
}


void LinkPositionView::onAttachedMenuRequest(MenuManager& menuManager)
{
    impl->onAttachedMenuRequest(menuManager);
}


void LinkPositionView::Impl::onAttachedMenuRequest(MenuManager& menu)
{
    menu.setPath("/").setPath(_("Target link type"));
    auto checkGroup = new ActionGroup(menu.topMenu());
    menu.addRadioItem(checkGroup, _("Any links"));
    menu.addRadioItem(checkGroup, _("IK priority link and root link"));
    menu.addRadioItem(checkGroup, _("IK priority link"));
    checkGroup->actions()[targetLinkTypeSelection.which()]->setChecked(true);
    checkGroup->sigTriggered().connect(
        [=](QAction* check){ setTargetLinkType(checkGroup->actions().indexOf(check)); });
                                           
    menu.setPath("/");
    menu.addSeparator();
    
    positionWidget->setOptionMenu(menu);

    menu.addSeparator();

    auto disableCustomIkCheck = menu.addCheckItem(_("Disable custom IK"));
    if(!kinematicsKit){
        disableCustomIkCheck->setEnabled(false);
    } else {
        disableCustomIkCheck->setChecked(kinematicsKit->isCustomIkDisabled());
        disableCustomIkCheck->sigToggled().connect(
            [&](bool on){
                if(kinematicsKit){
                    kinematicsKit->setCustomIkDisabled(on);
                    initializeConfigurationInterface();
                    updateDisplay();
                }
            });
    }
}
    

void LinkPositionView::Impl::setCoordinateModeInterfaceEnabled(bool on)
{
    for(auto& widget : coordinateModeWidgets){
        widget->setEnabled(on);
    }
    localCoordRadio.setEnabled(false);
}


void LinkPositionView::Impl::setCoordinateMode(int mode, bool doUpdateDisplay)
{
    coordinateModeGroup.blockSignals(true);
    
    if(mode == WorldCoordinateMode){
        worldCoordRadio.setEnabled(true);
        worldCoordRadio.setChecked(true);
        if(kinematicsKit){
            kinematicsKit->setCurrentBaseFrameType(LinkKinematicsKit::WorldFrame);
        }
    } else if(mode == BodyCoordinateMode){
        bodyCoordRadio.setEnabled(true);
        bodyCoordRadio.setChecked(true);
        if(kinematicsKit){
            kinematicsKit->setCurrentBaseFrameType(LinkKinematicsKit::BodyFrame);
        }
    } else if(mode == LocalCoordinateMode){
        localCoordRadio.setEnabled(true);
        localCoordRadio.setChecked(true);
    }

    coordinateModeGroup.blockSignals(false);

    if(mode != coordinateMode){
        coordinateMode = mode;
        updateCoordinateFrameCandidates();
    }

    if(doUpdateDisplay){
        updateDisplay();
    }
}


void LinkPositionView::Impl::setBodyCoordinateModeEnabled(bool on)
{
    bodyCoordRadio.setEnabled(on);
    if(!on && coordinateMode == BodyCoordinateMode){
        setCoordinateMode(WorldCoordinateMode, false);
    }
}


void LinkPositionView::Impl::onCoordinateModeRadioToggled(int mode)
{
    setCoordinateMode(mode, true);
    preferredCoordinateMode = mode;
}


void LinkPositionView::Impl::setTargetLinkType(int type)
{
    targetLinkTypeSelection.select(type);
    setTargetBodyAndLink(targetBodyItem, targetLink);
}


void LinkPositionView::Impl::setTargetBodyAndLink(BodyItem* bodyItem, Link* link)
{
    bool isTargetTypeChanged = (targetType != LinkTarget);
    bool isBodyItemChanged = isTargetTypeChanged || (bodyItem != targetBodyItem);
    bool isLinkChanged = isTargetTypeChanged || (link != targetLink);

    // Sub body's root link is recognized as the parent body's end link
    if(bodyItem && link){
        if(bodyItem->isAttachedToParentBody()){
            if(auto parentBodyItem = bodyItem->parentBodyItem()){
                link = bodyItem->body()->parentBodyLink();
                bodyItem = parentBodyItem;
            }
        }
        bool isIkLinkRequired = false;
        if(!targetLinkTypeSelection.is(AnyLink)){
            if(targetLinkTypeSelection.is(RootOrIkLink)){
                isIkLinkRequired = !link->isBodyRoot();
            } else {
                isIkLinkRequired = true;
            }
        }
        if(isIkLinkRequired){
            if(!bodyItem->findPresetIK(link)){
                LinkTraverse traverse(link);
                link = nullptr;
                for(int i=1; i < traverse.numLinks(); ++i){
                    if(bodyItem->findPresetIK(traverse[i])){
                        link = traverse[i];
                        break;
                    }
                }
            }
        }
    }

    if(!link){
        if(isLinkChanged){
            return;
        } else {
            isLinkChanged = true;
        }
    }
                
    if(isBodyItemChanged || isLinkChanged){

        if(isBodyItemChanged){
            positionWidget->clearPosition();
            targetConnections.disconnect();
            
            targetBodyItem = bodyItem;
    
            if(bodyItem){
                targetConnections.add(
                    bodyItem->sigNameChanged().connect(
                        [&](const std::string&){ updateTargetLink(targetLink); }));

                targetConnections.add(
                    bodyItem->sigKinematicStateChanged().connect(
                        [&](){ updateDisplayWithCurrentLinkPosition(); }));
            }
        }

        targetType = LinkTarget;
        updateTargetLink(link);
        updateDisplayWithCurrentLinkPosition();
    }
}


void LinkPositionView::Impl::updateTargetLink(Link* link)
{
    setCoordinateModeInterfaceEnabled(true);
    
    if(targetType != LinkTarget){
        return;
    }
    
    targetLink = link;
    kinematicsKit.reset();
    kinematicsKitConnection.reset();
    bool hasCoordinaeteFrames = false;
    
    if(!targetLink){
        targetLabel.setText("------");

    } else {
        auto body = targetBodyItem->body();

        targetLabel.setText(format("{0} / {1}", body->name(), targetLink->name()).c_str());

        if(defaultCoordName[WorldFrame].empty()){
            defaultCoordName[WorldFrame] = _("World Origin");
        }
        for(int i=0; i < 2; ++i){
            if(defaultCoordName[i].empty()){
                defaultCoordName[i] = _("Origin");
            }
        }

        kinematicsKit = targetBodyItem->getCurrentLinkKinematicsKit(targetLink);
        if(kinematicsKit){
            kinematicsKitConnection =
                kinematicsKit->sigFrameUpdate().connect(
                    [&](){ onFrameUpdate(); });
            if(functionToGetDefaultFrameNames){
                tie(defaultCoordName[WorldFrame], defaultCoordName[BodyFrame], defaultCoordName[LinkFrame]) =
                    functionToGetDefaultFrameNames(kinematicsKit);
            }
            if(kinematicsKit->frameSetSuite()){
                hasCoordinaeteFrames = true;
            }
            if(coordinateMode == WorldCoordinateMode){
                kinematicsKit->setCurrentBaseFrameType(LinkKinematicsKit::WorldFrame);
            } else {
                kinematicsKit->setCurrentBaseFrameType(LinkKinematicsKit::BodyFrame);
            }
            baseFrame = kinematicsKit->currentBaseFrame();
            linkFrame = kinematicsKit->currentLinkFrame();
        }
    }

    self->setEnabled(kinematicsKit != nullptr);
    setCoordinateFrameInterfaceEnabled(hasCoordinaeteFrames);
    resultLabel.setText("");

    updateCoordinateFrameCandidates();
    setCoordinateMode(preferredCoordinateMode, false);

    setBodyCoordinateModeEnabled(
        kinematicsKit && kinematicsKit->baseLink() && link != kinematicsKit->baseLink() && hasCoordinaeteFrames);

    initializeConfigurationInterface();
}


void LinkPositionView::Impl::setCoordinateFrameInterfaceEnabled(bool on)
{
    for(int i=0; i < 2; ++i){
        frameComboLabel[i].setEnabled(on);
        frameCombo[i].setEnabled(on);
        if(!on){
            frameCombo[i].clear();
        }
    }
}
    

void LinkPositionView::Impl::updateCoordinateFrameCandidates()
{
    for(int i=0; i < 2; ++i){
        updateCoordinateFrameCandidates(i);
    }
}


void LinkPositionView::Impl::updateCoordinateFrameCandidates(int frameComboIndex)
{
    int frameType;

    if(frameComboIndex == LinkFrameCombo){
        frameType = LinkFrame;
    } else {
        if(coordinateMode == WorldCoordinateMode){
            frameType = WorldFrame;
        } else {
            frameType = BodyFrame;
        }
    }

    CoordinateFrameSet* frames = nullptr;
    GeneralId currentFrameId = GeneralId::defaultId();

    if(kinematicsKit){
        frames = kinematicsKit->frameSet(frameType);
        currentFrameId = kinematicsKit->currentFrameId(frameType);
    }
    
    updateCoordinateFrameComboItems(
        frameCombo[frameComboIndex], frames, currentFrameId, defaultCoordName[frameType]);
}


void LinkPositionView::Impl::updateCoordinateFrameComboItems
(QComboBox& combo, CoordinateFrameSet* frames, const GeneralId& currentId, const std::string& originLabel)
{
    constexpr bool EnableToolTip = false;
    
    combo.clear();
    combo.addItem(QString("0: %1").arg(originLabel.c_str()), 0);
    if(EnableToolTip){
        combo.setItemData(0, QString(), Qt::ToolTipRole);
    }
    int currentIndex = 0;

    if(frames){
        auto candidates = frames->getFindableFrameLists();
        const int n = candidates.size();
        for(int i=0; i < n; ++i){
            int index = combo.count();
            auto frame = candidates[i];
            auto& id = frame->id();
            if(id.isInt()){
                combo.addItem(QString("%1: %2").arg(id.toInt()).arg(frame->note().c_str()), id.toInt());
                if(EnableToolTip){
                    combo.setItemData(index, QString(), Qt::ToolTipRole);
                }
            } else {
                combo.addItem(id.label().c_str(), id.toString().c_str());
                if(EnableToolTip){
                    combo.setItemData(index, frame->note().c_str(), Qt::ToolTipRole);
                }
            }
            if(id == currentId){
                currentIndex = index;
            }
        }
    }

    combo.setCurrentIndex(currentIndex);
}


void LinkPositionView::Impl::onFrameComboActivated(int frameComboIndex, int index)
{
    GeneralId id;
    auto idValue = frameCombo[frameComboIndex].itemData(index);
    if(idValue.userType() == QMetaType::Int){
        id = idValue.toInt();
    } else if(idValue.userType() == QMetaType::QString){
        id = idValue.toString().toStdString();
    }
    if(id.isValid()){
        int frameType;
        if(frameComboIndex == BaseFrameCombo){
            if(coordinateMode == WorldCoordinateMode){
                frameType = WorldFrame;
            } else {
                frameType = BodyFrame;
            }
        } else {
            frameType = LinkFrame;
        }
        if(kinematicsKit){
            kinematicsKit->setCurrentFrame(frameType, id);
            if(frameComboIndex == BaseFrameCombo){
                baseFrame = kinematicsKit->currentFrame(frameType);
            } else {
                linkFrame = kinematicsKit->currentLinkFrame();
            }
            kinematicsKit->notifyFrameUpdate();
        }
        updateDisplay();
    }
}


void LinkPositionView::Impl::onFrameUpdate()
{
    bool coordinateModeUpdated = false;
    auto baseFrameType = kinematicsKit->currentBaseFrameType();
    
    if(baseFrameType == LinkCoordFrameSetSuite::WorldFrame && coordinateMode != WorldCoordinateMode){
        setCoordinateMode(WorldCoordinateMode, false);
        preferredCoordinateMode = WorldCoordinateMode;

    } else if(baseFrameType == LinkCoordFrameSetSuite::BodyFrame && coordinateMode != BodyCoordinateMode){
        setCoordinateMode(BodyCoordinateMode, false);
        preferredCoordinateMode = BodyCoordinateMode;

    } else {
        updateCoordinateFrameCandidates();
    }

    for(int i=0; i < 2; ++i){
        GeneralId newId;
        if(i == BaseFrameCombo){
            newId = kinematicsKit->currentBaseFrameId();
        } else {
            newId = kinematicsKit->currentLinkFrameId();
        }
        auto& combo = frameCombo[i];
        int currentIndex = combo.currentIndex();
        for(int j=0; j < combo.count(); ++j){
            GeneralId id;
            auto idValue = combo.itemData(j);
            if(idValue.userType() == QMetaType::Int){
                id = idValue.toInt();
            } else if(idValue.userType() == QMetaType::QString){
                id = idValue.toString().toStdString();
            }
            if(id == newId){
                currentIndex = j;
                break;
            }
        }
        if(currentIndex != combo.currentIndex()){
            combo.setCurrentIndex(currentIndex);
        }
    }

    baseFrame = kinematicsKit->currentBaseFrame();
    linkFrame = kinematicsKit->currentLinkFrame();
    
    updateDisplay();
}


void LinkPositionView::Impl::setConfigurationInterfaceEnabled(bool on)
{
    for(auto& widget : configurationWidgets){
        widget->setEnabled(on);
    }
    if(!on){
        configurationLabel.setText("-----");
    }
}
    

void LinkPositionView::Impl::initializeConfigurationInterface()
{
    currentConfigurationTypes.clear();

    bool isConfigurationValid = false;
    if(kinematicsKit && !kinematicsKit->isCustomIkDisabled()){
        if(auto configurationHandler = kinematicsKit->configurationHandler()){
            isConfigurationValid = true;
        }
    }

    setConfigurationInterfaceEnabled(isConfigurationValid);

    if(configurationDialog){
        if(isConfigurationValid){
            configurationDialog->updateConfigurationTypes();
        } else {
            configurationDialog->reset();
        }
    }

}


void LinkPositionView::Impl::showConfigurationDialog()
{
    if(!configurationDialog){
        configurationDialog = new JointSpaceConfigurationDialog(this);
    }
    
    if(configurationDialog->updateConfigurationTypes()){
        if(configurationDialog->isHidden()){
            configurationDialog->show();
            if(!configurationDialog->lastPosition.isNull()){
                configurationDialog->treeWidget.setSizeAdjustPolicy(
                    QAbstractScrollArea::AdjustIgnored);
                configurationDialog->setGeometry(configurationDialog->lastPosition);
            }
        }
    }
}


JointSpaceConfigurationDialog::JointSpaceConfigurationDialog(LinkPositionView::Impl* view)
    : Dialog(view->self, Qt::Tool),
      view(view)
{
    setSizeGripEnabled(true);
    
    auto vbox = new QVBoxLayout;

    auto hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel(_("Search")));
    searchBox.setEnabled(false);
    hbox->addWidget(&searchBox, 1);
    feasibleCheck.setText(_("Feasible"));
    feasibleCheck.sigToggled().connect([&](bool){ updateItemDisplay(); });
    hbox->addWidget(&feasibleCheck);
    vbox->addLayout(hbox);

    auto header = treeWidget.header();
    header->setMinimumSectionSize(1);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionsClickable(true);
    QObject::connect(header, &QHeaderView::sectionClicked,
                     [&](int index){ onSectionClicked(index); });

    treeWidget.setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    treeWidget.setRootIsDecorated(false);

    treeWidget.sigCurrentItemChanged().connect(
        [&](QTreeWidgetItem* item, QTreeWidgetItem*){
            if(item){
                int id = item->data(0, Qt::UserRole).toInt();
                applyConfiguration(id);
            }
        });
    vbox->addWidget(&treeWidget);

    hbox = new QHBoxLayout;
    auto updateButton = new PushButton(_("&Update"));
    updateButton->sigClicked().connect([&](){ updateConfigurationStates(); });
    hbox->addWidget(updateButton);
    hbox->addStretch();

    auto applyButton = new PushButton(_("&Apply"));
    applyButton->setDefault(true);
    auto cancelButton = new PushButton(_("&Cancel"));
    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(applyButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, &QDialogButtonBox::rejected, [&](){ onCanceled(); });
    hbox->addWidget(buttonBox);

    vbox->addLayout(hbox);
    
    setLayout(vbox);
}


void JointSpaceConfigurationDialog::reset()
{
    body.reset();
    jointPath.reset();
    configuration.reset();
    treeWidget.clear();
    lastSortedSection = -1;
}


bool JointSpaceConfigurationDialog::updateConfigurationTypes()
{
    reset();

    auto& kinematicsKit = view->kinematicsKit;
    if(!kinematicsKit){
        return false;
    }
    body = kinematicsKit->body();
    jointPath = kinematicsKit->jointPath();
    configuration = kinematicsKit->configurationHandler();
    if(!jointPath || !configuration){
        return false;
    }

    T0 = jointPath->endLink()->T();
    bodyState0.storePositions(*body);
    
    auto name = jointPath->name();
    if(name.empty()){
        setWindowTitle(_("Joint-space configuration"));
    } else {
        setWindowTitle(format(_("{} configuration"), name).c_str());
    }

    int n = configuration->getNumConfigurationTypes();
    auto targetNames = configuration->getConfigurationTargetNames();
    treeWidget.setColumnCount(targetNames.size() + 1);
    QStringList headerLabels;
    headerLabels.append("No");
    for(auto& label : targetNames){
        headerLabels.append(label.c_str());
    }
    treeWidget.setHeaderLabels(headerLabels);
    
    for(int i=0; i < n; ++i){
        int typeId = configuration->getConfigurationTypeId(i);
        auto labels = configuration->getConfigurationStateNames(typeId);
        auto item = new QTreeWidgetItem;
        item->setData(0, Qt::DisplayRole, i + 1);
        item->setTextAlignment(0, Qt::AlignCenter);
        item->setData(0, Qt::UserRole, typeId);
        for(size_t j=0; j < labels.size(); ++j){
            item->setText(j + 1, labels[j].c_str());
        }
        treeWidget.addTopLevelItem(item);
    }

    updateConfigurationStates();

    return true;
}


void JointSpaceConfigurationDialog::updateConfigurationStates()
{
    int n = treeWidget.topLevelItemCount();
    if(n == 0){
        return;
    }
    
    T0 = jointPath->endLink()->T();
    bodyState0.storePositions(*body);

    for(int i=0; i < n; ++i){
        auto item = treeWidget.topLevelItem(i);
        int id = item->data(0, Qt::UserRole).toInt();
        configuration->setPreferredConfigurationType(id);
        bool solved = jointPath->calcInverseKinematics(T0);
        if(solved){
            for(auto& joint : jointPath->joints()){
                if(joint->q() > joint->q_upper() || joint->q() < joint->q_lower()){
                    solved = false;
                    break;
                }
            }
        }
        item->setData(0, Qt::UserRole + 1, solved);
    }

    configuration->resetPreferredConfigurationType();
    bodyState0.restorePositions(*body);
    jointPath->endLink()->T() = T0;

    updateItemDisplay();
}


void JointSpaceConfigurationDialog::updateItemDisplay()
{
    int n = treeWidget.topLevelItemCount();
    if(n == 0){
        return;
    }
    QBrush red(Qt::red);
    QVariant none;
    for(int i=0; i < n; ++i){
        auto item = treeWidget.topLevelItem(i);
        bool solved = item->data(0, Qt::UserRole + 1).toBool();
        for(int j=0; j < item->columnCount(); ++j){
            if(solved){
                item->setData(j, Qt::ForegroundRole, none);
            } else {
                item->setForeground(j, red);
            }
            if(feasibleCheck.isChecked()){
                item->setHidden(!solved);
            } else {
                item->setHidden(false);
            }
        }
    }
    /*
    auto index1 = treeWidget.indexFromItem(treeWidget.topLevelItem(0), 0);
    auto index2 = treeWidget.indexFromItem(treeWidget.topLevelItem(n - 1), treeWidget.columnCount() - 1);
    treeWidget.model()->dataChanged(index1, index2);
    */
    // treeWidget.update();
    // treeWidget.repaint();
}


void JointSpaceConfigurationDialog::onSectionClicked(int index)
{
    Qt::SortOrder order;
    if(index != lastSortedSection){
        order = Qt::AscendingOrder;
    } else {
        order = (lastSortOrder != Qt::AscendingOrder) ? Qt::AscendingOrder : Qt::DescendingOrder;
    }
    treeWidget.sortByColumn(index, order);
    
    lastSortedSection = index;
    lastSortOrder = order;
}


void JointSpaceConfigurationDialog::applyConfiguration(int id)
{
    configuration->setPreferredConfigurationType(id);
    view->findBodyIkSolution(view->kinematicsKit->link()->T(), true);
    configuration->resetPreferredConfigurationType();
}


void JointSpaceConfigurationDialog::onCanceled()
{
    if(body){
        bodyState0.restorePositions(*body);
        jointPath->endLink()->T() = T0;
        view->targetBodyItem->notifyKinematicStateChange();
    }
        
    hide();
}


void JointSpaceConfigurationDialog::hideEvent(QHideEvent* event)
{
    lastPosition = geometry();
    Dialog::hideEvent(event);
}


QSize ConfTreeWidget::sizeHint() const
{
    int c = topLevelItemCount();
    if(c == 0){
        return TreeWidget::sizeHint();
    }
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    auto header_ = header();
    auto r = visualItemRect(topLevelItem(c - 1));
    return QSize(-1, r.bottom() + header_->height() + frameWidth * 2 + r.height() / 2);
}


bool LinkPositionView::Impl::setPositionEditTarget(AbstractPositionEditTarget* target)
{
    positionWidget->clearPosition();
    targetConnections.disconnect();

    targetType = PositionEditTarget;
    positionEditTarget = target;
    baseFrame = identityFrame;
    linkFrame = identityFrame;

    targetConnections.add(
        target->sigPositionChanged().connect(
            [&](const Position&){ updateDisplayWithPositionEditTarget(); }));

    targetConnections.add(
        target->sigPositionEditTargetExpired().connect(
            [&](){ onPositionEditTargetExpired(); }));

    targetLabel.setText(target->getPositionName().c_str());
    self->setEnabled(target->isEditable());
    setCoordinateFrameInterfaceEnabled(false);
    setConfigurationInterfaceEnabled(false);
    setBodyCoordinateModeEnabled(false);
    setCoordinateModeInterfaceEnabled(false);

    updateDisplayWithPositionEditTarget();

    return true;
}


void LinkPositionView::Impl::onPositionEditTargetExpired()
{

}


void LinkPositionView::Impl::updateDisplay()
{
    userInputConnections.block();
    
    if(targetType == LinkTarget){
        updateDisplayWithCurrentLinkPosition();

    } else if(targetType == PositionEditTarget){
        updateDisplayWithPositionEditTarget();
    }

    userInputConnections.unblock();

    resultLabel.setText(_("Actual State"));
    resultLabel.setStyleSheet(normalStyle);
}


void LinkPositionView::Impl::updateDisplayWithCurrentLinkPosition()
{
    if(targetLink){
        Position T = baseFrame->T().inverse(Eigen::Isometry) * targetLink->Ta() * linkFrame->T();
        if(kinematicsKit){
            if(coordinateMode == BodyCoordinateMode && kinematicsKit->baseLink()){
                T = kinematicsKit->baseLink()->Ta().inverse(Eigen::Isometry) * T;
            }
            positionWidget->setReferenceRpy(kinematicsKit->referenceRpy());
        }
        positionWidget->setPosition(T);
        updateConfigurationDisplay();
    }
}


void LinkPositionView::Impl::updateDisplayWithPositionEditTarget()
{
    if(positionEditTarget){
        positionWidget->setReferenceRpy(Vector3::Zero());
        positionWidget->setPosition(positionEditTarget->getPosition());
    }
}


void LinkPositionView::Impl::updateConfigurationDisplay()
{
    if(kinematicsKit){
        if(auto configuration = kinematicsKit->configurationHandler()){
            configurationString.clear();
            auto types = configuration->getCurrentConfigurationTypes();
            if(types != currentConfigurationTypes){
                size_t numTypes = types.size();
                vector<vector<string>> allLabels(numTypes);
                int maxNumLabels = 0;
                for(int i=0; i < numTypes; ++i){
                    allLabels[i] = configuration->getConfigurationStateNames(types[i]);
                    int n = allLabels[i].size();
                    if(n > maxNumLabels){
                        maxNumLabels = n;
                    }
                }
                for(size_t i=0; i < maxNumLabels; ++i){
                    for(size_t j=0; j < numTypes; ++j){
                        auto& labels = allLabels[j];
                        if(i < labels.size()){
                            auto& label = labels[i];
                            auto p = tmpConfigurationLabelSet.insert(label);
                            if(p.second){ // newly inserted
                                if(!configurationString.empty()){
                                    //configurationString.append(" / ");
                                    configurationString.append("-");
                                }
                                configurationString.append(label);
                            }
                        }
                    }
                }
                configurationLabel.setText(configurationString.c_str());
                currentConfigurationTypes = types;
            }
            tmpConfigurationLabelSet.clear();
        }
    }
}


bool LinkPositionView::Impl::applyPositionInput(const Position& T)
{
    bool accepted = false;
    
    if(targetType == LinkTarget){
        accepted = findBodyIkSolution(T, false);

    } else if(targetType == PositionEditTarget){
        accepted = applyInputToPositionEditTarget(T);
    }

    return accepted;
}


bool LinkPositionView::Impl::findBodyIkSolution(const Position& T_input, bool isRawT)
{
    shared_ptr<InverseKinematics> ik;
    if(kinematicsKit){
        ik = kinematicsKit->inverseKinematics();
    }
    if(!ik){
        return false;
    }
    
    bool solved = false;
    
    kinematicsKit->setReferenceRpy(positionWidget->getRpyInput());

    targetBodyItem->beginKinematicStateEdit();

    if(isRawT){
        solved = ik->calcInverseKinematics(T_input);
    } else {
        Position T = baseFrame->T() * T_input * linkFrame->T().inverse(Eigen::Isometry);
        if(coordinateMode == BodyCoordinateMode && kinematicsKit->baseLink()){
            T = kinematicsKit->baseLink()->Ta() * T;
        }
        T.linear() = targetLink->calcRfromAttitude(T.linear());
        solved = ik->calcInverseKinematics(T);
    }

    if(solved){
        ik->calcRemainingPartForwardKinematicsForInverseKinematics();
        targetBodyItem->notifyKinematicStateChange();
        targetBodyItem->acceptKinematicStateEdit();
        resultLabel.setText(_("Solved"));
        resultLabel.setStyleSheet(normalStyle);
    } else {
        targetBodyItem->cancelKinematicStateEdit();
        resultLabel.setText(_("Not Solved"));
        resultLabel.setStyleSheet(errorStyle);
    }

    return solved;
}


bool LinkPositionView::Impl::applyInputToPositionEditTarget(const Position& T_input)
{
    bool accepted = false;
    
    if(positionEditTarget){

        targetConnections.block();
        accepted = positionEditTarget->setPosition(T_input);
        targetConnections.unblock();

        if(accepted){
            resultLabel.setText(_("Accepted"));
            resultLabel.setStyleSheet(normalStyle);
        } else {
            resultLabel.setText(_("Not Accepted"));
            resultLabel.setStyleSheet(errorStyle);
        }
    }

    return accepted;
}


bool LinkPositionView::storeState(Archive& archive)
{
    return impl->storeState(archive);
}


bool LinkPositionView::Impl::storeState(Archive& archive)
{
    archive.write("target_link_type", targetLinkTypeSelection.selectedSymbol());
    coordinateModeSelection.select(coordinateMode);
    archive.write("coordinate_mode", coordinateModeSelection.selectedSymbol());
    coordinateModeSelection.select(preferredCoordinateMode);
    archive.write("preferred_coordinate_mode", coordinateModeSelection.selectedSymbol());

    positionWidget->storeState(archive);

    return true;
}


bool LinkPositionView::restoreState(const Archive& archive)
{
    return impl->restoreState(archive);
}


bool LinkPositionView::Impl::restoreState(const Archive& archive)
{
    userInputConnections.block();
    
    string symbol;

    if(archive.read("target_link_type", symbol)){
        targetLinkTypeSelection.select(symbol);
    }
    if(archive.read("preferred_coordinate_mode", symbol)){
        if(coordinateModeSelection.select(symbol)){
            preferredCoordinateMode = coordinateModeSelection.which();
        }
    }
    if(archive.read("coordinate_mode", symbol)){
        if(coordinateModeSelection.select(symbol)){
            setCoordinateMode(coordinateModeSelection.which(), true);
        }
    }

    positionWidget->restoreState(archive);
    
    userInputConnections.unblock();

    return true;
}
