/***************************************************************************
 *   Copyright (c) 2017 Abdullah Tahiri <abdullah.tahiri.yo@gmail.com>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#endif

#include <App/Application.h>
#include <Base/Console.h>
#include <Gui/Action.h>
#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/CommandT.h>
#include <Gui/Document.h>
#include <Gui/MainWindow.h>
#include <Gui/Notifications.h>
#include <Gui/Selection/Selection.h>
#include <Gui/Selection/SelectionObject.h>

#include "DrawSketchHandler.h"
#include "ViewProviderSketch.h"

#include <Mod/Part/App/Geometry.h>
#include <Mod/Sketcher/App/SketchObject.h>

#include "Utils.h"


using namespace std;
using namespace SketcherGui;
using namespace Sketcher;

bool isSketcherVirtualSpaceActive(Gui::Document* doc, bool actsOnSelection)
{
    if (doc) {
        // checks if a Sketch Viewprovider is in Edit and is in no special mode
        if (doc->getInEdit()
            && doc->getInEdit()->isDerivedFrom<SketcherGui::ViewProviderSketch>()) {
            if (static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit())->getSketchMode()
                == ViewProviderSketch::STATUS_NONE) {
                if (!actsOnSelection) {
                    return true;
                }
                else if (Gui::Selection().countObjectsOfType<Sketcher::SketchObject>() > 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

void ActivateVirtualSpaceHandler(Gui::Document* doc, DrawSketchHandler* handler)
{
    std::unique_ptr<DrawSketchHandler> ptr(handler);
    if (doc) {
        if (doc->getInEdit()
            && doc->getInEdit()->isDerivedFrom<SketcherGui::ViewProviderSketch>()) {
            SketcherGui::ViewProviderSketch* vp =
                static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
            vp->purgeHandler();
            vp->activateHandler(std::move(ptr));
        }
    }
}

// Show/Hide B-spline degree
DEF_STD_CMD_A(CmdSketcherSwitchVirtualSpace)

CmdSketcherSwitchVirtualSpace::CmdSketcherSwitchVirtualSpace()
    : Command("Sketcher_SwitchVirtualSpace")
{
    sAppModule = "Sketcher";
    sGroup = "Sketcher";
    sMenuText = QT_TR_NOOP("Switch Virtual Space");
    sToolTipText =
        QT_TR_NOOP("Switches the selected constraints or the view to the other virtual space");
    sWhatsThis = "Sketcher_SwitchVirtualSpace";
    sStatusTip = sToolTipText;
    sPixmap = "Sketcher_SwitchVirtualSpace";
    sAccel = "Z, Z";
    eType = ForEdit;
}

void CmdSketcherSwitchVirtualSpace::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    bool modeChange = true;

    std::vector<Gui::SelectionObject> selection;

    if (Gui::Selection().countObjectsOfType<Sketcher::SketchObject>() > 0) {
        // Now we check whether we have a constraint selected or not.
        selection = getSelection().getSelectionEx();

        // only one sketch with its subelements are allowed to be selected
        if (selection.size() != 1
            || !selection[0].isObjectTypeOf(Sketcher::SketchObject::getClassTypeId())) {
            Gui::TranslatedUserWarning(getActiveGuiDocument(),
                                       QObject::tr("Wrong selection"),
                                       QObject::tr("Select constraints from the sketch."));
            return;
        }

        // get the needed lists and objects
        const std::vector<std::string>& SubNames = selection[0].getSubNames();
        if (SubNames.empty()) {
            Gui::TranslatedUserWarning(getActiveGuiDocument(),
                                       QObject::tr("Wrong selection"),
                                       QObject::tr("Select constraints from the sketch."));
            return;
        }

        for (std::vector<std::string>::const_iterator it = SubNames.begin(); it != SubNames.end();
             ++it) {
            // see if we have constraints, if we do it is not a mode change, but a toggle.
            if (it->size() > 10 && it->substr(0, 10) == "Constraint") {
                modeChange = false;
            }
        }
    }

    if (modeChange) {
        Gui::Document* doc = getActiveGuiDocument();

        SketcherGui::ViewProviderSketch* vp =
            static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
        vp->setIsShownVirtualSpace(!vp->getIsShownVirtualSpace());
    }
    // toggle the selected constraint(s)
    else {
        // get the needed lists and objects
        const std::vector<std::string>& SubNames = selection[0].getSubNames();
        if (SubNames.empty()) {
            Gui::TranslatedUserWarning(getActiveGuiDocument(),
                                       QObject::tr("Wrong selection"),
                                       QObject::tr("Select constraints from the sketch."));

            return;
        }

        SketcherGui::ViewProviderSketch* sketchgui =
            static_cast<SketcherGui::ViewProviderSketch*>(getActiveGuiDocument()->getInEdit());
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();

        // undo command open
        openCommand(QT_TRANSLATE_NOOP("Command", "Toggle constraints to the other virtual space"));

        int successful = SubNames.size();
        // go through the selected subelements
        for (std::vector<std::string>::const_iterator it = SubNames.begin(); it != SubNames.end();
             ++it) {
            // only handle constraints
            if (it->size() > 10 && it->substr(0, 10) == "Constraint") {
                int ConstrId = Sketcher::PropertyConstraintList::getIndexFromConstraintName(*it);
                Gui::Command::openCommand(
                    QT_TRANSLATE_NOOP("Command", "Update constraint's virtual space"));
                try {
                    Gui::cmdAppObjectArgs(Obj, "toggleVirtualSpace(%d)", ConstrId);
                }
                catch (const Base::Exception&) {
                    successful--;
                }
            }
        }

        if (successful > 0) {
            commitCommand();
        }
        else {
            abortCommand();
        }

        // recomputer and clear the selection (convenience)
        tryAutoRecompute(Obj);
        getSelection().clearSelection();
    }
}

bool CmdSketcherSwitchVirtualSpace::isActive()
{
    return isSketcherVirtualSpaceActive(getActiveGuiDocument(), false);
}

void CreateSketcherCommandsVirtualSpace()
{
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdSketcherSwitchVirtualSpace());
}
