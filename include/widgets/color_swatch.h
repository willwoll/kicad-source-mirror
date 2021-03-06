/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef COLOR_SWATCH__H_
#define COLOR_SWATCH__H_

#include <wx/bitmap.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/statbmp.h>
#include <wx/window.h>

#include <functional>

#include <gal/color4d.h>


enum SWATCH_SIZE
{
    SWATCH_SMALL,
    SWATCH_MEDIUM,
    SWATCH_LARGE,
    SWATCH_EXPAND
};


const static wxSize SWATCH_SIZE_SMALL_DU( 8, 6 );
const static wxSize SWATCH_SIZE_MEDIUM_DU( 24, 10 );
const static wxSize SWATCH_SIZE_LARGE_DU( 24, 16 );
const static wxSize CHECKERBOARD_SIZE_DU( 3, 3 );


/**
 * Class representing a simple color swatch, of the kind used to
 * set layer colors
 */
class COLOR_SWATCH: public wxPanel
{
public:

    /**
     * Construct a COLOR_SWATCH
     *
     * @param aParent parent window
     * @param aColor initial swatch color
     * @param aID id to use when sending swatch events
     */
    COLOR_SWATCH( wxWindow* aParent, KIGFX::COLOR4D aColor, int aID, KIGFX::COLOR4D aBackground,
                  const KIGFX::COLOR4D aDefault, SWATCH_SIZE aSwatchType );

    /**
     * constructor for wxFormBuilder
     */
    COLOR_SWATCH( wxWindow *aParent, wxWindowID aId, const wxPoint &aPos = wxDefaultPosition,
                  const wxSize &aSize = wxDefaultSize, long aStyle = 0 );

    /**
     * Set the current swatch color directly.
     */
    void SetSwatchColor( KIGFX::COLOR4D aColor, bool aSendEvent );

    /**
     * Sets the color that will be chosen with the "Reset to Default" button in the chooser
     */
    void SetDefaultColor( KIGFX::COLOR4D aColor );

    /**
     * Set the swatch background color.
     */
    void SetSwatchBackground( KIGFX::COLOR4D aBackground );

    /**
     * @return the current swatch color
     */
    KIGFX::COLOR4D GetSwatchColor() const;

    /**
     * Prompt for a new colour, using the colour picker dialog.
     *
     * A colour change event will be sent if it's set.
     */
    void GetNewSwatchColor();

    void SetReadOnly( bool aReadOnly = true ) { m_readOnly = aReadOnly; }
    bool IsReadOnly() const { return m_readOnly; }

    /// Registers a handler for when the user tries to interact with a read-only swatch
    void SetReadOnlyCallback( std::function<void()> aCallback ) { m_readOnlyCallback = aCallback; }

    static wxBitmap MakeBitmap( KIGFX::COLOR4D aColor, KIGFX::COLOR4D aBackground, wxSize aSize,
                                wxSize aCheckerboardSize, KIGFX::COLOR4D aCheckerboardBackground );

private:
    void setupEvents();

    /**
     * Pass unwanted events on to listeners of this object
     */
    void rePostEvent( wxEvent& aEvent );

    KIGFX::COLOR4D  m_color;
    KIGFX::COLOR4D  m_background;
    KIGFX::COLOR4D  m_default;

    wxStaticBitmap* m_swatch;

    wxSize          m_size;
    wxSize          m_checkerboardSize;
    KIGFX::COLOR4D  m_checkerboardBg;

    /// A read-only swatch won't show the color chooser dialog but otherwise works normally
    bool            m_readOnly;
    std::function<void()> m_readOnlyCallback;
};


/**
 * Event signalling a swatch has changed color
 */
wxDECLARE_EVENT(COLOR_SWATCH_CHANGED, wxCommandEvent);

#endif // COLOR_SWATCH__H_
