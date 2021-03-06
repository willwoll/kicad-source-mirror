/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 CERN
 * Copyright (C) 2019-2020 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <kiway.h>
#include <sch_painter.h>
#include <tool/tool_manager.h>
#include <tools/ee_actions.h>
#include <tools/lib_control.h>
#include <symbol_edit_frame.h>
#include <lib_view_frame.h>
#include <symbol_tree_model_adapter.h>
#include <wildcards_and_files_ext.h>
#include <gestfich.h>
#include <confirm.h>


bool LIB_CONTROL::Init()
{
    m_frame = getEditFrame<SCH_BASE_FRAME>();
    m_selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
    m_isSymbolEditor = m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR );

    if( m_isSymbolEditor )
    {
        CONDITIONAL_MENU& ctxMenu = m_menu.GetMenu();
        SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();

        auto libSelectedCondition = [ editFrame ] ( const SELECTION& aSel ) {
            LIB_ID sel = editFrame->GetTreeLIBID();
            return !sel.GetLibNickname().empty() && sel.GetLibItemName().empty();
        };
        auto pinnedLibSelectedCondition = [ editFrame ] ( const SELECTION& aSel ) {
            LIB_TREE_NODE* current = editFrame->GetCurrentTreeNode();
            return current && current->m_Type == LIB_TREE_NODE::LIB && current->m_Pinned;
        };
        auto unpinnedLibSelectedCondition = [ editFrame ] (const SELECTION& aSel ) {
            LIB_TREE_NODE* current = editFrame->GetCurrentTreeNode();
            return current && current->m_Type == LIB_TREE_NODE::LIB && !current->m_Pinned;
        };
        auto symbolSelectedCondition = [ editFrame ] ( const SELECTION& aSel ) {
            LIB_ID sel = editFrame->GetTreeLIBID();
            return !sel.GetLibNickname().empty() && !sel.GetLibItemName().empty();
        };

        ctxMenu.AddItem( ACTIONS::pinLibrary,            unpinnedLibSelectedCondition );
        ctxMenu.AddItem( ACTIONS::unpinLibrary,          pinnedLibSelectedCondition );
        ctxMenu.AddSeparator();

        ctxMenu.AddItem( ACTIONS::save,                  libSelectedCondition );
        ctxMenu.AddItem( ACTIONS::saveAs,                libSelectedCondition );
        ctxMenu.AddItem( ACTIONS::revert,                libSelectedCondition );

        ctxMenu.AddSeparator();
        ctxMenu.AddItem( EE_ACTIONS::newSymbol,          libSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::editSymbol,         symbolSelectedCondition );

        ctxMenu.AddSeparator();
        ctxMenu.AddItem( ACTIONS::save,                  symbolSelectedCondition );
        ctxMenu.AddItem( ACTIONS::saveCopyAs,            symbolSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::duplicateSymbol,    symbolSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::deleteSymbol,       symbolSelectedCondition );
        ctxMenu.AddItem( ACTIONS::revert,                symbolSelectedCondition );

        ctxMenu.AddSeparator();
        ctxMenu.AddItem( EE_ACTIONS::cutSymbol,          symbolSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::copySymbol,         symbolSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::pasteSymbol,        libSelectedCondition );

        ctxMenu.AddSeparator();
        ctxMenu.AddItem( EE_ACTIONS::importSymbol,       libSelectedCondition );
        ctxMenu.AddItem( EE_ACTIONS::exportSymbol,       symbolSelectedCondition );
    }

    return true;
}


int LIB_CONTROL::AddLibrary( const TOOL_EVENT& aEvent )
{
    bool createNew = aEvent.IsAction( &ACTIONS::newLibrary );

    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
        static_cast<SYMBOL_EDIT_FRAME*>( m_frame )->AddLibraryFile( createNew );

    return 0;
}


int LIB_CONTROL::EditSymbol( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );
        int                unit = 0;
        LIB_ID             partId = editFrame->GetTreeLIBID( &unit );

        editFrame->LoadPart( partId.GetLibItemName(), partId.GetLibNickname(), unit );
    }

    return 0;
}


int LIB_CONTROL::AddSymbol( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );

        if( aEvent.IsAction( &EE_ACTIONS::newSymbol ) )
            editFrame->CreateNewPart();
        else if( aEvent.IsAction( &EE_ACTIONS::importSymbol ) )
            editFrame->ImportPart();
    }

    return 0;
}


int LIB_CONTROL::Save( const TOOL_EVENT& aEvt )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );

        if( aEvt.IsAction( &EE_ACTIONS::save ) )
            editFrame->Save();
        else if( aEvt.IsAction( &EE_ACTIONS::saveAs ) || aEvt.IsAction( &EE_ACTIONS::saveCopyAs ) )
            editFrame->SaveAs();
        else if( aEvt.IsAction( &EE_ACTIONS::saveAll ) )
            editFrame->SaveAll();
    }

    return 0;
}


int LIB_CONTROL::Revert( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
        static_cast<SYMBOL_EDIT_FRAME*>( m_frame )->Revert();

    return 0;
}


int LIB_CONTROL::ExportSymbol( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
        static_cast<SYMBOL_EDIT_FRAME*>( m_frame )->ExportPart();

    return 0;
}


int LIB_CONTROL::CutCopyDelete( const TOOL_EVENT& aEvt )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );

        if( aEvt.IsAction( &EE_ACTIONS::cutSymbol ) || aEvt.IsAction( &EE_ACTIONS::copySymbol ) )
            editFrame->CopyPartToClipboard();

        if( aEvt.IsAction( &EE_ACTIONS::cutSymbol ) || aEvt.IsAction( &EE_ACTIONS::deleteSymbol ) )
            editFrame->DeletePartFromLibrary();
    }

    return 0;
}


int LIB_CONTROL::DuplicateSymbol( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );
        editFrame->DuplicatePart( aEvent.IsAction( &EE_ACTIONS::pasteSymbol ) );
    }

    return 0;
}


int LIB_CONTROL::OnDeMorgan( const TOOL_EVENT& aEvent )
{
    int convert = aEvent.IsAction( &EE_ACTIONS::showDeMorganStandard ) ?
            LIB_ITEM::LIB_CONVERT::BASE : LIB_ITEM::LIB_CONVERT::DEMORGAN;

    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        m_toolMgr->RunAction( ACTIONS::cancelInteractive, true );
        m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

        SYMBOL_EDIT_FRAME* libEditFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );
        libEditFrame->SetConvert( convert );

        m_toolMgr->ResetTools( TOOL_BASE::MODEL_RELOAD );
        libEditFrame->RebuildView();
    }
    else if( m_frame->IsType( FRAME_SCH_VIEWER ) || m_frame->IsType( FRAME_SCH_VIEWER_MODAL ) )
    {
        LIB_VIEW_FRAME* libViewFrame = static_cast<LIB_VIEW_FRAME*>( m_frame );
        libViewFrame->SetUnitAndConvert( libViewFrame->GetUnit(), convert );
    }

    return 0;
}


int LIB_CONTROL::PinLibrary( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );
        LIB_TREE_NODE*  currentNode = editFrame->GetCurrentTreeNode();

        if( currentNode && !currentNode->m_Pinned )
        {
            currentNode->m_Pinned = true;
            editFrame->RegenerateLibraryTree();
        }
    }

    return 0;
}


int LIB_CONTROL::UnpinLibrary( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        SYMBOL_EDIT_FRAME* editFrame = static_cast<SYMBOL_EDIT_FRAME*>( m_frame );
        LIB_TREE_NODE*  currentNode = editFrame->GetCurrentTreeNode();

        if( currentNode && currentNode->m_Pinned )
        {
            currentNode->m_Pinned = false;
            editFrame->RegenerateLibraryTree();
        }
    }

    return 0;
}


int LIB_CONTROL::ShowComponentTree( const TOOL_EVENT& aEvent )
{
    if( m_frame->IsType( FRAME_SCH_SYMBOL_EDITOR ) )
    {
        wxCommandEvent dummy;
        static_cast<SYMBOL_EDIT_FRAME*>( m_frame )->OnToggleSearchTree( dummy );
    }

    return 0;
}


int LIB_CONTROL::ShowElectricalTypes( const TOOL_EVENT& aEvent )
{
    KIGFX::SCH_RENDER_SETTINGS* renderSettings = m_frame->GetRenderSettings();
    renderSettings->m_ShowPinsElectricalType = !renderSettings->m_ShowPinsElectricalType;

    // Update canvas
    m_frame->GetCanvas()->GetView()->UpdateAllItems( KIGFX::REPAINT );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int LIB_CONTROL::ToggleSyncedPinsMode( const TOOL_EVENT& aEvent )
{
    if( !m_isSymbolEditor )
        return 0;

    SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();
    editFrame->m_SyncPinEdit = !editFrame->m_SyncPinEdit;

    return 0;
}


int LIB_CONTROL::ExportView( const TOOL_EVENT& aEvent )
{
    if( !m_isSymbolEditor )
        return 0;

    SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();
    LIB_PART*          part = editFrame->GetCurPart();

    if( !part )
    {
        wxMessageBox( _( "No symbol to export" ) );
        return 0;
    }

    wxString   file_ext = wxT( "png" );
    wxString   mask = wxT( "*." ) + file_ext;
    wxFileName fn( part->GetName() );
    fn.SetExt( "png" );

    wxString projectPath = wxPathOnly( m_frame->Prj().GetProjectFullName() );

    wxFileDialog dlg( editFrame, _( "Image File Name" ), projectPath, fn.GetFullName(),
                      PngFileWildcard(), wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() == wxID_OK && !dlg.GetPath().IsEmpty() )
    {
        // calling wxYield is mandatory under Linux, after closing the file selector dialog
        // to refresh the screen before creating the PNG or JPEG image from screen
        wxYield();

        if( !SaveCanvasImageToFile( editFrame, dlg.GetPath(), wxBITMAP_TYPE_PNG ) )
        {
            wxMessageBox( wxString::Format( _( "Can't save file \"%s\"." ), dlg.GetPath() ) );
        }
    }

    return 0;
}


int LIB_CONTROL::ExportSymbolAsSVG( const TOOL_EVENT& aEvent )
{
    if( !m_isSymbolEditor )
        return 0;

    SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();
    LIB_PART*          part = editFrame->GetCurPart();

    if( !part )
    {
        wxMessageBox( _( "No symbol to export" ) );
        return 0;
    }

    wxString   file_ext = wxT( "svg" );
    wxString   mask     = wxT( "*." ) + file_ext;
    wxFileName fn( part->GetName() );
    fn.SetExt( file_ext );

    wxString pro_dir = wxPathOnly( m_frame->Prj().GetProjectFullName() );

    wxString fullFileName = EDA_FILE_SELECTOR( _( "Filename:" ), pro_dir, fn.GetFullName(),
                                               file_ext, mask, m_frame, wxFD_SAVE, true );

    if( !fullFileName.IsEmpty() )
    {
        PAGE_INFO pageSave = editFrame->GetScreen()->GetPageSettings();
        PAGE_INFO pageTemp = pageSave;

        wxSize componentSize = part->GetUnitBoundingBox( editFrame->GetUnit(),
                                                         editFrame->GetConvert() ).GetSize();

        // Add a small margin to the plot bounding box
        pageTemp.SetWidthMils(  int( componentSize.x * 1.2 ) );
        pageTemp.SetHeightMils( int( componentSize.y * 1.2 ) );

        editFrame->GetScreen()->SetPageSettings( pageTemp );
        editFrame->SVGPlotSymbol( fullFileName );
        editFrame->GetScreen()->SetPageSettings( pageSave );
    }

    return 0;
}


int LIB_CONTROL::AddSymbolToSchematic( const TOOL_EVENT& aEvent )
{
    LIB_PART* part = nullptr;
    LIB_ID    libId;
    int       unit, convert;

    if( m_isSymbolEditor )
    {
        SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();

        part = editFrame->GetCurPart();
        unit = editFrame->GetUnit();
        convert = editFrame->GetConvert();

        if( part )
            libId = part->GetLibId();
    }
    else
    {
        LIB_VIEW_FRAME* viewFrame = getEditFrame<LIB_VIEW_FRAME>();

        if( viewFrame->IsModal() )
        {
            // if we're modal then we just need to return the symbol selection; the caller is
            // already in a EE_ACTIONS::placeSymbol coroutine.
            viewFrame->FinishModal();
            return 0;
        }
        else
        {
            part    = viewFrame->GetSelectedSymbol();
            unit    = viewFrame->GetUnit();
            convert = viewFrame->GetConvert();

            if( part )
                libId = part->GetLibId();
        }
    }

    if( part )
    {
        SCH_EDIT_FRAME* schframe = (SCH_EDIT_FRAME*) m_frame->Kiway().Player( FRAME_SCH, false );

        if( !schframe )      // happens when the schematic editor is not active (or closed)
        {
            DisplayErrorMessage( m_frame, _( "No schematic currently open." ) );
            return 0;
        }

        wxCHECK( part->GetLibId().IsValid(), 0 );

        SCH_COMPONENT* comp = new SCH_COMPONENT( *part, libId, &schframe->GetCurrentSheet(), unit,
                                                 convert );

        comp->SetParent( schframe->GetCurrentSheet().LastScreen() );

        if( schframe->eeconfig()->m_AutoplaceFields.enable )
            comp->AutoplaceFields( /* aScreen */ nullptr, /* aManual */ false );

        schframe->Raise();
        schframe->GetToolManager()->RunAction( EE_ACTIONS::placeSymbol, true, comp );
    }

    return 0;
}


int LIB_CONTROL::UpdateSymbolInSchematic( const TOOL_EVENT& aEvent )
{
    wxCHECK( m_isSymbolEditor, 0 );

    SYMBOL_EDIT_FRAME* editFrame = getEditFrame<SYMBOL_EDIT_FRAME>();

    wxCHECK( editFrame, 0 );

    LIB_PART* currentPart = editFrame->GetCurPart();

    wxCHECK( currentPart, 0 );

    SCH_EDIT_FRAME* schframe = (SCH_EDIT_FRAME*) m_frame->Kiway().Player( FRAME_SCH, false );

    if( !schframe )      // happens when the schematic editor is not active (or closed)
    {
        DisplayErrorMessage( m_frame, _( "No schematic currently open." ) );
        return 0;
    }

    schframe->UpdateSymbolFromEditor( *currentPart );

    SCH_SCREEN* currentScreen = editFrame->GetScreen();

    wxCHECK( currentScreen, 0 );

    currentScreen->ClrModify();

    return 0;
}


void LIB_CONTROL::setTransitions()
{
    Go( &LIB_CONTROL::AddLibrary,            ACTIONS::newLibrary.MakeEvent() );
    Go( &LIB_CONTROL::AddLibrary,            ACTIONS::addLibrary.MakeEvent() );
    Go( &LIB_CONTROL::AddSymbol,             EE_ACTIONS::newSymbol.MakeEvent() );
    Go( &LIB_CONTROL::AddSymbol,             EE_ACTIONS::importSymbol.MakeEvent() );
    Go( &LIB_CONTROL::EditSymbol,            EE_ACTIONS::editSymbol.MakeEvent() );

    Go( &LIB_CONTROL::Save,                  ACTIONS::save.MakeEvent() );
    Go( &LIB_CONTROL::Save,                  ACTIONS::saveAs.MakeEvent() );     // for libraries
    Go( &LIB_CONTROL::Save,                  ACTIONS::saveCopyAs.MakeEvent() ); // for symbols
    Go( &LIB_CONTROL::Save,                  ACTIONS::saveAll.MakeEvent() );
    Go( &LIB_CONTROL::Revert,                ACTIONS::revert.MakeEvent() );
    Go( &LIB_CONTROL::UpdateSymbolInSchematic,
            EE_ACTIONS::saveInSchematic.MakeEvent() );

    Go( &LIB_CONTROL::DuplicateSymbol,       EE_ACTIONS::duplicateSymbol.MakeEvent() );
    Go( &LIB_CONTROL::CutCopyDelete,         EE_ACTIONS::deleteSymbol.MakeEvent() );
    Go( &LIB_CONTROL::CutCopyDelete,         EE_ACTIONS::cutSymbol.MakeEvent() );
    Go( &LIB_CONTROL::CutCopyDelete,         EE_ACTIONS::copySymbol.MakeEvent() );
    Go( &LIB_CONTROL::DuplicateSymbol,       EE_ACTIONS::pasteSymbol.MakeEvent() );
    Go( &LIB_CONTROL::ExportSymbol,          EE_ACTIONS::exportSymbol.MakeEvent() );
    Go( &LIB_CONTROL::ExportView,            EE_ACTIONS::exportSymbolView.MakeEvent() );
    Go( &LIB_CONTROL::ExportSymbolAsSVG,     EE_ACTIONS::exportSymbolAsSVG.MakeEvent() );
    Go( &LIB_CONTROL::AddSymbolToSchematic,  EE_ACTIONS::addSymbolToSchematic.MakeEvent() );

    Go( &LIB_CONTROL::OnDeMorgan,            EE_ACTIONS::showDeMorganStandard.MakeEvent() );
    Go( &LIB_CONTROL::OnDeMorgan,            EE_ACTIONS::showDeMorganAlternate.MakeEvent() );

    Go( &LIB_CONTROL::ShowElectricalTypes,   EE_ACTIONS::showElectricalTypes.MakeEvent() );
    Go( &LIB_CONTROL::PinLibrary,            ACTIONS::pinLibrary.MakeEvent() );
    Go( &LIB_CONTROL::UnpinLibrary,          ACTIONS::unpinLibrary.MakeEvent() );
    Go( &LIB_CONTROL::ShowComponentTree,     EE_ACTIONS::showComponentTree.MakeEvent() );
    Go( &LIB_CONTROL::ToggleSyncedPinsMode,  EE_ACTIONS::toggleSyncedPinsMode.MakeEvent() );
}
