/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
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

#ifndef KICAD_H
#define KICAD_H


#include <wx/process.h>
#include <eda_base_frame.h>
#include <kiway_player.h>

class TREEPROJECTFILES;
class TREE_PROJECT_FRAME;
class ACTION_TOOLBAR;
class KICAD_SETTINGS;

// Identify the type of files handled by KiCad manager
//
// When changing this enum  please verify (and perhaps update)
// TREE_PROJECT_FRAME::GetFileExt(),
// s_AllowedExtensionsToList[]

enum TreeFileType {
    TREE_ROOT = 0,
    TREE_LEGACY_PROJECT,    // Legacy project file (.pro)
    TREE_JSON_PROJECT,      // JSON formatted project file (.kicad_pro)
    TREE_LEGACY_SCHEMATIC,  // Schematic file (.sch)
    TREE_SEXPR_SCHEMATIC,   // Schematic file (.kicad_sch)
    TREE_LEGACY_PCB,        // board file (.brd) legacy format
    TREE_SEXPR_PCB,         // board file (.kicad_brd) new s expression format
    TREE_GERBER,            // Gerber  file (.pho, .g*)
    TREE_GERBER_JOB_FILE,   // Gerber  file (.gbrjob)
    TREE_HTML,              // HTML file (.htm, *.html)
    TREE_PDF,               // PDF file (.pdf)
    TREE_TXT,               // ascii text file (.txt)
    TREE_NET,               // netlist file (.net)
    TREE_UNKNOWN,
    TREE_DIRECTORY,
    TREE_CMP_LINK,          // cmp/footprint link file (.cmp)
    TREE_REPORT,            // report file (.rpt)
    TREE_FP_PLACE,          // footprints position (place) file (.pos)
    TREE_DRILL,             // Excellon drill file (.drl)
    TREE_DRILL_NC,          // Similar Excellon drill file (.nc)
    TREE_DRILL_XNC,         // Similar Excellon drill file (.xnc)
    TREE_SVG,               // SVG file (.svg)
    TREE_PAGE_LAYOUT_DESCR, // Page layout and title block descr file (.kicad_wks)
    TREE_FOOTPRINT_FILE,    // footprint file (.kicad_mod)
    TREE_SCHEMATIC_LIBFILE, // schematic library file (.lib)
    TREE_SEXPR_SYMBOL_LIB_FILE, // s-expression symbol library file (.kicad_sym)
    TREE_MAX
};


/**
 * The main KiCad project manager frame.  It is not a KIWAY_PLAYER.
 */
class KICAD_MANAGER_FRAME : public EDA_BASE_FRAME
{
public:
    KICAD_MANAGER_FRAME( wxWindow* parent, const wxString& title,
                         const wxPoint& pos, const wxSize& size );

    ~KICAD_MANAGER_FRAME();

    void OnIdle( wxIdleEvent& event );

    bool canCloseWindow( wxCloseEvent& aCloseEvent ) override;
    void doCloseWindow() override;
    void OnSize( wxSizeEvent& event );

    void OnArchiveFiles( wxCommandEvent& event );
    void OnUnarchiveFiles( wxCommandEvent& event );

    void OnOpenFileInTextEditor( wxCommandEvent& event );
    void OnBrowseInFileExplorer( wxCommandEvent& event );

    void OnFileHistory( wxCommandEvent& event );
    void OnClearFileHistory( wxCommandEvent& aEvent );
    void OnExit( wxCommandEvent& event );

    void ReCreateMenuBar() override;
    void RecreateBaseHToolbar();
    void RecreateLauncher();

    wxString GetCurrentFileName() const override
    {
        return GetProjectFileName();
    }


    /**
     * @brief Creates a project and imports a non-KiCad Schematic and PCB
     * @param aWindowTitle to display to the user when opening the files
     * @param aFilesWildcard that includes both PCB and Schematic files (from wildcards_and_files_ext.h)
     * @param aSchFileExtension e.g. "sch" or "csa"
     * @param aPcbFileExtension e.g. "brd" or "cpa"
     * @param aSchFileType Type of Schematic File to import (from SCH_IO_MGR::SCH_FILE_T)
     * @param aPcbFileType Type of PCB File to import (from IO_MGR::PCB_FILE_T)
    */
    void ImportNonKiCadProject( wxString aWindowTitle, wxString aFilesWildcard,
            wxString aSchFileExtension, wxString aPcbFileExtension, int aSchFileType,
            int aPcbFileType );

    /**
     *  Open dialog to import CADSTAR Schematic and PCB Archive files.
     */
    void OnImportCadstarArchiveFiles( wxCommandEvent& event );

    /**
     *  Open dialog to import Eagle schematic and board files.
     */
    void OnImportEagleFiles( wxCommandEvent& event );

    /**
     * Displays \a aText in the text panel.
     *
     * @param aText The text to display.
     */
    void PrintMsg( const wxString& aText );

    /**
     * Prints the current working directory name and the project name on the text panel.
     */
    void PrintPrjInfo();

    /**
     * Erase the text panel.
     */
    void ClearMsg();

    void RefreshProjectTree();

    /**
     * Creates a new project by setting up and initial project, schematic, and board files.
     *
     * The project file is copied from the kicad.pro template file if possible.  Otherwise,
     * a minimal project file is created from an empty project.  A minimal schematic and
     * board file are created to prevent the schematic and board editors from complaining.
     * If any of these files already exist, they are not overwritten.
     *
     * @param aProjectFileName is the absolute path of the project file name.
     * @param aCreateStubFiles specifies if an empty PCB and schematic should be created
     */
    void CreateNewProject( const wxFileName& aProjectFileName, bool aCreateStubFiles = true );

    /**
     * Closes the project, and saves it if aSave is true;
     */
    bool CloseProject( bool aSave );
    void LoadProject( const wxFileName& aProjectFileName );


    void LoadSettings( APP_SETTINGS_BASE* aCfg ) override;

    void SaveSettings( APP_SETTINGS_BASE* aCfg ) override;

    void ShowChangedLanguage() override;
    void CommonSettingsChanged( bool aEnvVarsChanged, bool aTextVarsChanged ) override;
    void ProjectChanged() override;

    /**
     * Called by sending a event with id = ID_INIT_WATCHED_PATHS
     * rebuild the list of watched paths
     */
    void OnChangeWatchedPaths( wxCommandEvent& aEvent );

    void InstallPreferences( PAGED_DIALOG* aParent, PANEL_HOTKEYS_EDITOR* aHotkeysPanel ) override;

    const wxString GetProjectFileName() const;

    bool IsProjectActive();
    // read only accessors
    const wxString SchFileName();
    const wxString SchLegacyFileName();
    const wxString PcbFileName();
    const wxString PcbLegacyFileName();

    void ReCreateTreePrj();

    wxWindow* GetToolCanvas() const override;

    DECLARE_EVENT_TABLE()

protected:
    virtual void setupUIConditions() override;

private:
    void setupTools();
    void setupActions();

    APP_SETTINGS_BASE* config() const override;

    KICAD_SETTINGS* kicadSettings() const;

    const SEARCH_STACK& sys_search() override;

    wxString help_name() override;

    void language_change( wxCommandEvent& event );

    bool m_openSavedWindows;

private:
    TREE_PROJECT_FRAME* m_leftWin;
    ACTION_TOOLBAR*     m_launcher;
    wxTextCtrl*         m_messagesBox;
    ACTION_TOOLBAR*     m_mainToolBar;

    int                 m_leftWinWidth;
    bool                m_active_project;
};


// The C++ project manager includes a single PROJECT in its link image.
class PROJECT;
extern PROJECT& Prj();

#endif
