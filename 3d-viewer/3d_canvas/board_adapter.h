/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015-2016 Mario Luzeiro <mrluzeiro@ua.pt>
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef BOARD_ADAPTER_H
#define BOARD_ADAPTER_H

#include <array>
#include <vector>
#include "../3d_rendering/3d_render_raytracing/accelerators/ccontainer2d.h"
#include "../3d_rendering/3d_render_raytracing/accelerators/ccontainer.h"
#include "../3d_rendering/3d_render_raytracing/shapes3D/cbbox.h"
#include "../3d_rendering/ccamera.h"
#include "../3d_enums.h"
#include "../3d_cache/3d_cache.h"
#include "../common_ogl/cogl_att_list.h"

#include <layers_id_colors_and_visibility.h>
#include <pad.h>
#include <track.h>
#include <wx/gdicmn.h>
#include <pcb_base_frame.h>
#include <pcb_text.h>
#include <pcb_shape.h>
#include <dimension.h>
#include <zone.h>
#include <footprint.h>
#include <reporter.h>

class COLOR_SETTINGS;

/// A type that stores a container of 2d objects for each layer id
typedef std::map< PCB_LAYER_ID, CBVHCONTAINER2D *> MAP_CONTAINER_2D;

/// A type that stores polysets for each layer id
typedef std::map< PCB_LAYER_ID, SHAPE_POLY_SET *> MAP_POLY;

/// This defines the range that all coord will have to be rendered.
/// It will use this value to convert to a normalized value between
/// -(RANGE_SCALE_3D/2) .. +(RANGE_SCALE_3D/2)
#define RANGE_SCALE_3D 8.0f


/**
 *  Class BOARD_ADAPTER
 *  Helper class to handle information needed to display 3D board
 */
class BOARD_ADAPTER
{
 public:

    BOARD_ADAPTER();

    ~BOARD_ADAPTER();

    /**
     * @brief Set3DCacheManager - Update the Cache manager pointer
     * @param aCachePointer: the pointer to the 3d cache manager
     */
    void Set3DCacheManager( S3D_CACHE *aCachePointer ) noexcept
    {
        m_3d_model_manager = aCachePointer;
    }

    /**
     * @brief Get3DCacheManager - Return the 3d cache manager pointer
     * @return
     */
    S3D_CACHE *Get3DCacheManager( ) const noexcept
    {
        return m_3d_model_manager;
    }

    /**
     * @brief GetFlag - get a configuration status of a flag
     * @param aFlag: the flag to get the status
     * @return true if flag is set, false if not
     */
    bool GetFlag( DISPLAY3D_FLG aFlag ) const ;

    /**
     * @brief SetFlag - set the status of a flag
     * @param aFlag: the flag to get the status
     * @param aState: status to set
     */
    void SetFlag( DISPLAY3D_FLG aFlag, bool aState );

    /**
     * @brief Is3DLayerEnabled - Check if a layer is enabled
     * @param aLayer: layer ID to get status
     */
    bool Is3DLayerEnabled( PCB_LAYER_ID aLayer ) const;

    /**
     * @brief ShouldFPBeDisplayed - Test if footprint should be displayed in relation to
     * attributes and the flags
     */
    bool ShouldFPBeDisplayed( FOOTPRINT_ATTR_T aFPAttributes ) const;

    /**
     * @brief SetBoard - Set current board to be rendered
     * @param aBoard: board to process
     */
    void SetBoard( BOARD *aBoard ) noexcept
    {
        m_board = aBoard;
    }

    /**
     * @brief GetBoard - Get current board to be rendered
     * @return BOARD pointer
     */
    const BOARD *GetBoard() const noexcept
    {
        return m_board;
    }

    void SetColorSettings( COLOR_SETTINGS* aSettings ) noexcept
    {
        m_colors = aSettings;
    }

    /**
     * @brief InitSettings - Function to be called by the render when it need to
     * reload the settings for the board.
     * @param aStatusReporter: the pointer for the status reporter
     * @param aWarningReporter: pointer for the warning reporter
     */
    void InitSettings( REPORTER* aStatusReporter, REPORTER* aWarningReporter );

    /**
     * @brief BiuTo3Dunits - Board integer units To 3D units
     * @return the conversion factor to transform a position from the board to 3d units
     */
    double BiuTo3Dunits() const noexcept
    {
        return m_biuTo3Dunits;
    }

    /**
     * @brief GetBBox3DU - Get the bbox of the pcb board
     * @return the board bbox in 3d units
     */
    const CBBOX &GetBBox3DU() const noexcept
    {
        return m_boardBoundingBox;
    }

    /**
     * @brief GetEpoxyThickness3DU - Get the current epoxy thickness
     * @return thickness in 3d units
     */
    float GetEpoxyThickness3DU() const noexcept
    {
        return m_epoxyThickness3DU;
    }

    /**
     * @brief GetNonCopperLayerThickness3DU - Get the current non copper layers thickness
     * @return thickness in 3d units of non copperlayers
     */
    float GetNonCopperLayerThickness3DU() const noexcept
    {
        return m_nonCopperLayerThickness3DU;
    }

    /**
     * @brief GetCopperThickness3DU - Get the current copper layer thickness
     * @return thickness in 3d units of copperlayers
     */
    float GetCopperThickness3DU() const noexcept
    {
        return m_copperThickness3DU;
    }

    /**
     * @brief GetCopperThicknessBIU - Get the current copper layer thickness
     * @return thickness in board units
     */
    int GetHolePlatingThicknessBIU() const noexcept;

    /**
     * @brief GetBoardSizeBIU - Get the board size
     * @return size in BIU units
     */
    wxSize GetBoardSizeBIU() const noexcept
    {
        return m_boardSize;
    }

    /**
     * @brief GetBoardPosBIU - Get the board center
     * @return position in BIU units
     */
    wxPoint GetBoardPosBIU() const noexcept
    {
        return m_boardPos;
    }

    /**
     * @brief GetBoardCenter - the board center position in 3d units
     * @return board center vector position in 3d units
     */
    const SFVEC3F &GetBoardCenter3DU() const noexcept
    {
        return m_boardCenter;
    }

    /**
     * @brief GetModulesZcoord3DIU - Get the position of the footprint in 3d integer units
     * considering if it is flipped or not.
     * @param aIsFlipped: true for use in footprints on Front (top) layer, false
     *                    if footprint is on back (bottom) layer
     * @return the Z position of 3D shapes, in 3D integer units
     */
    float GetModulesZcoord3DIU( bool aIsFlipped ) const ;

    /**
     * @brief GridGet - get the current grid
     * @return space type of the grid
     */
    GRID3D_TYPE GridGet() const noexcept
    {
        return m_3D_grid_type;
    }

    /**
     * @brief GridSet - set the current grid
     * @param aGridType = the type space of the grid
     */
    void GridSet( GRID3D_TYPE aGridType ) noexcept
    {
        m_3D_grid_type = aGridType;
    }

    /**
     * @brief GridGet - get the current antialiasing mode value
     * @return antialiasing mode value
     */
    ANTIALIASING_MODE AntiAliasingGet() const { return m_antialiasing_mode; }

    /**
     * @brief AntiAliasingSet - set the current antialiasing mode value
     * @param aAAmode = antialiasing mode value
     */
    void AntiAliasingSet( ANTIALIASING_MODE aAAmode ) { m_antialiasing_mode = aAAmode; }

    /**
     * @brief RenderEngineSet
     * @param aRenderEngine = the render engine mode selected
     */
    void RenderEngineSet( RENDER_ENGINE aRenderEngine ) noexcept
    {
        m_render_engine = aRenderEngine;
    }

    /**
     * @brief RenderEngineGet
     * @return render engine on use
     */
    RENDER_ENGINE RenderEngineGet() const noexcept
    {
        return m_render_engine;
    }

    /**
     * @brief MaterialModeSet
     * @param aMaterialMode = the render material mode
     */
    void MaterialModeSet( MATERIAL_MODE aMaterialMode ) noexcept
    {
        m_material_mode = aMaterialMode;
    }

    /**
     * @brief MaterialModeGet
     * @return material rendering mode
     */
    MATERIAL_MODE MaterialModeGet() const noexcept
    {
        return m_material_mode;
    }

    /**
     * @brief GetBoardPoly - Get the current polygon of the epoxy board
     * @return the shape polygon
     */
    const SHAPE_POLY_SET &GetBoardPoly() const noexcept
    {
        return m_board_poly;
    }

    /**
     * @brief GetLayerColor - get the technical color of a layer
     * @param aLayerId: the layer to get the color information
     * @return the color in SFVEC3F format
     */
    SFVEC4F GetLayerColor( PCB_LAYER_ID aLayerId ) const;

    /**
     * @brief GetItemColor - get the technical color of a layer
     * @param aItemId: the item id to get the color information
     * @return the color in SFVEC3F format
     */
    SFVEC4F GetItemColor( int aItemId ) const;

    /**
     * @brief GetColor
     * @param aColor: the color mapped
     * @return the color in SFVEC3F format
     */
    SFVEC4F GetColor( COLOR4D aColor ) const;

    /**
     * @brief GetLayerTopZpos3DU - Get the top z position
     * @param aLayerId: layer id
     * @return position in 3D units
     */
    float GetLayerTopZpos3DU( PCB_LAYER_ID aLayerId ) const noexcept
    {
        return m_layerZcoordTop[aLayerId];
    }

    /**
     * @brief GetLayerBottomZpos3DU - Get the bottom z position
     * @param aLayerId: layer id
     * @return position in 3D units
     */
    float GetLayerBottomZpos3DU( PCB_LAYER_ID aLayerId ) const noexcept
    {
        return m_layerZcoordBottom[aLayerId];
    }

    /**
     * @brief GetMapLayers - Get the map of container that have the objects per layer
     * @return the map containers of this board
     */
    const MAP_CONTAINER_2D &GetMapLayers() const noexcept
    {
        return m_layers_container2D;
    }

    const CBVHCONTAINER2D* GetPlatedPads_Front() const noexcept
    {
        return m_platedpads_container2D_F_Cu;
    }

    const CBVHCONTAINER2D* GetPlatedPads_Back() const noexcept
    {
        return m_platedpads_container2D_B_Cu;
    }

    /**
     * @brief GetMapLayersHoles -Get the map of container that have the holes per layer
     * @return the map containers of holes from this board
     */
    const MAP_CONTAINER_2D &GetMapLayersHoles() const noexcept
    {
        return m_layers_holes2D;
    }

    /**
     * @brief GetThroughHole_Outer - Get the inflated ThroughHole container
     * @return a container with holes
     */
    const CBVHCONTAINER2D &GetThroughHole_Outer() const noexcept
    {
        return m_through_holes_outer;
    }

    /**
     * @brief GetThroughHole_Outer_Ring - Get the ThroughHole container that
     * include the width of the annular ring.
     * @return a container with holes.
     */
    const CBVHCONTAINER2D& GetThroughHole_Outer_Ring() const noexcept
    {
        return m_through_holes_outer_ring;
    }

    const SHAPE_POLY_SET &GetThroughHole_Outer_poly() const noexcept
    {
        return m_through_outer_holes_poly;
    }

    const SHAPE_POLY_SET &GetThroughHole_Outer_Ring_poly() const noexcept
    {
        return m_through_outer_ring_holes_poly;
    }

    const SHAPE_POLY_SET &GetThroughHole_Outer_poly_NPTH() const noexcept
    {
        return m_through_outer_holes_poly_NPTH;
    }

    /**
     * @brief GetThroughHole_Vias_Outer -
     * @return a container with via THT holes only
     */
    const CBVHCONTAINER2D &GetThroughHole_Vias_Outer() const noexcept
    {
        return m_through_holes_vias_outer;
    }

    const SHAPE_POLY_SET &GetThroughHole_Vias_Outer_poly() const noexcept
    {
        return m_through_outer_holes_vias_poly;
    }

    /**
     * @brief GetThroughHole_Inner - Get the ThroughHole container
     * @return a container with holes
     */
    const CBVHCONTAINER2D &GetThroughHole_Inner() const noexcept
    {
        return m_through_holes_inner;
    }

    /**
     * @brief GetStats_Nr_Vias - Get statistics of the nr of vias
     * @return number of vias
     */
    unsigned int GetStats_Nr_Vias() const noexcept
    {
        return m_stats_nr_vias;
    }

    /**
     * @brief GetStats_Nr_Holes - Get statistics of the nr of holes
     * @return number of holes
     */
    unsigned int GetStats_Nr_Holes() const noexcept
    {
        return m_stats_nr_holes;
    }

    /**
     * @brief GetStats_Med_Via_Hole_Diameter3DU - Average diameter of the via holes
     * @return dimension in 3D units
     */
    float GetStats_Med_Via_Hole_Diameter3DU() const noexcept
    {
        return m_stats_via_med_hole_diameter;
    }

    /**
     * @brief GetStats_Med_Hole_Diameter3DU - Average diameter of holes
     * @return dimension in 3D units
     */
    float GetStats_Med_Hole_Diameter3DU() const noexcept
    {
        return m_stats_hole_med_diameter;
    }

    /**
     * @brief GetStats_Med_Track_Width - Average width of the tracks
     * @return dimensions in 3D units
     */
    float GetStats_Med_Track_Width() const noexcept
    {
        return m_stats_track_med_width;
    }

    /**
     * @brief GetNrSegmentsCircle
     * @param aDiameter3DU: diameter in 3DU
     * @return number of sides that should be used in that circle
     */
    unsigned int GetNrSegmentsCircle( float aDiameter3DU ) const;

    /**
     * @brief GetNrSegmentsCircle
     * @param aDiameterBIU: diameter in board internal units
     * @return number of sides that should be used in that circle
     */
    unsigned int GetNrSegmentsCircle( int aDiameterBIU ) const;

    /**
     * @brief GetPolyMap - Get maps of polygons's layers
     * @return the map with polygons's layers
     */
    const MAP_POLY &GetPolyMap() const noexcept
    {
        return m_layers_poly;
    }

    const SHAPE_POLY_SET *GetPolyPlatedPads_Front()
    {
        return m_F_Cu_PlatedPads_poly;
    }

    const SHAPE_POLY_SET *GetPolyPlatedPads_Back()
    {
        return m_B_Cu_PlatedPads_poly;
    }

    const MAP_POLY &GetPolyMapHoles_Inner() const noexcept
    {
        return m_layers_inner_holes_poly;
    }

    const MAP_POLY &GetPolyMapHoles_Outer() const noexcept
    {
        return m_layers_outer_holes_poly;
    }

 private:
    /**
     * Create the board outline polygon.
     *
     * @return false if the outline could not be created
     */
    bool createBoardPolygon( wxString* aErrorMsg );
    void createLayers( REPORTER* aStatusReporter );
    void destroyLayers();

    // Helper functions to create the board
     void createNewTrack( const TRACK* aTrack, CGENERICCONTAINER2D *aDstContainer,
                          int aClearanceValue );

    void createNewPadWithClearance( const PAD *aPad, CGENERICCONTAINER2D *aDstContainer,
                                    PCB_LAYER_ID aLayer, wxSize aClearanceValue ) const;

    COBJECT2D *createNewPadDrill( const PAD* aPad, int aInflateValue );

    void AddPadsWithClearanceToContainer( const FOOTPRINT *aFootprint,
                                          CGENERICCONTAINER2D *aDstContainer,
                                          PCB_LAYER_ID aLayerId, int aInflateValue,
                                          bool aSkipNPTHPadsWihNoCopper, bool aSkipPlatedPads,
                                          bool aSkipNonPlatedPads );

    void AddFPShapesWithClearanceToContainer( const FOOTPRINT *aFootprint,
                                              CGENERICCONTAINER2D *aDstContainer,
                                              PCB_LAYER_ID aLayerId, int aInflateValue );

    void AddShapeWithClearanceToContainer( const PCB_TEXT *aText,
                                           CGENERICCONTAINER2D *aDstContainer,
                                           PCB_LAYER_ID aLayerId, int aClearanceValue );

    void AddShapeWithClearanceToContainer( const PCB_SHAPE *aShape,
                                           CGENERICCONTAINER2D *aDstContainer,
                                           PCB_LAYER_ID aLayerId, int aClearanceValue );

    void AddShapeWithClearanceToContainer( const DIMENSION_BASE *aDimension,
                                           CGENERICCONTAINER2D *aDstContainer,
                                           PCB_LAYER_ID aLayerId, int aClearanceValue );

    void AddSolidAreasShapesToContainer( const ZONE *aZoneContainer,
                                         CGENERICCONTAINER2D *aDstContainer,
                                         PCB_LAYER_ID aLayerId );

    void TransformArcToSegments( const wxPoint &aCentre, const wxPoint &aStart, double aArcAngle,
                                 int aCircleToSegmentsCount, int aWidth,
                                 CGENERICCONTAINER2D *aDstContainer, const BOARD_ITEM &aBoardItem );

    void buildPadShapeThickOutlineAsSegments( const PAD *aPad, CGENERICCONTAINER2D *aDstContainer,
                                              int aWidth );

    // Helper functions to create poly contours
    void buildPadShapeThickOutlineAsPolygon( const PAD *aPad, SHAPE_POLY_SET &aCornerBuffer,
                                             int aWidth) const;

    void transformFPShapesToPolygon( const FOOTPRINT *aFootprint, PCB_LAYER_ID aLayer,
                                     SHAPE_POLY_SET& aCornerBuffer ) const;

public:
    SFVEC4F m_BgColorBot;         ///< background bottom color
    SFVEC4F m_BgColorTop;         ///< background top color
    SFVEC4F m_BoardBodyColor;     ///< in realistic mode: FR4 board color
    SFVEC4F m_SolderMaskColorBot; ///< in realistic mode: solder mask color ( bot )
    SFVEC4F m_SolderMaskColorTop; ///< in realistic mode: solder mask color ( top )
    SFVEC4F m_SolderPasteColor;   ///< in realistic mode: solder paste color
    SFVEC4F m_SilkScreenColorBot; ///< in realistic mode: SilkScreen color ( bot )
    SFVEC4F m_SilkScreenColorTop; ///< in realistic mode: SilkScreen color ( top )
    SFVEC4F m_CopperColor;        ///< in realistic mode: copper color

    SFVEC3F m_opengl_selectionColor;

    // Raytracing light colors

    SFVEC3F m_raytrace_lightColorCamera;
    SFVEC3F m_raytrace_lightColorTop;
    SFVEC3F m_raytrace_lightColorBottom;

    std::vector<SFVEC3F> m_raytrace_lightColor;
    std::vector<SFVEC2F> m_raytrace_lightSphericalCoords;

    // Raytracing options
    int m_raytrace_nrsamples_shadows;
    int m_raytrace_nrsamples_reflections;
    int m_raytrace_nrsamples_refractions;

    float m_raytrace_spread_shadows;
    float m_raytrace_spread_reflections;
    float m_raytrace_spread_refractions;

    int m_raytrace_recursivelevel_reflections;
    int m_raytrace_recursivelevel_refractions;

private:

    BOARD*              m_board;
    S3D_CACHE*          m_3d_model_manager;
    COLOR_SETTINGS*     m_colors;

    // Render options

    std::vector< bool > m_drawFlags;
    GRID3D_TYPE         m_3D_grid_type;
    RENDER_ENGINE       m_render_engine;
    MATERIAL_MODE       m_material_mode;
    ANTIALIASING_MODE   m_antialiasing_mode;


    // Pcb board position

    /// center board actual position in board units
    wxPoint m_boardPos;

    /// board actual size in board units
    wxSize  m_boardSize;

    /// 3d center position of the pcb board in 3d units
    SFVEC3F m_boardCenter;


    // Pcb board bounding boxes

    /// 3d bounding box of the pcb board in 3d units
    CBBOX             m_boardBoundingBox;

    /// It contains polygon contours for each layer
    MAP_POLY          m_layers_poly;

    SHAPE_POLY_SET*   m_F_Cu_PlatedPads_poly;
    SHAPE_POLY_SET*   m_B_Cu_PlatedPads_poly;

    /// It contains polygon contours for holes of each layer (outer holes)
    MAP_POLY          m_layers_outer_holes_poly;

    /// It contains polygon contours for holes of each layer (inner holes)
    MAP_POLY          m_layers_inner_holes_poly;

    /// It contains polygon contours for (just) non plated through holes (outer cylinder)
    SHAPE_POLY_SET    m_through_outer_holes_poly_NPTH;

    /// It contains polygon contours for through holes (outer cylinder)
    SHAPE_POLY_SET    m_through_outer_holes_poly;

    /// It contains polygon contours for through holes vias (outer cylinder)
    SHAPE_POLY_SET    m_through_outer_holes_vias_poly;

    /// It contains polygon contours for through holes vias (outer annular ring)
    SHAPE_POLY_SET    m_through_outer_ring_holes_poly;

    /// PCB board outline polygon
    SHAPE_POLY_SET    m_board_poly;


    // 2D element containers

    /// It contains the 2d elements of each layer
    MAP_CONTAINER_2D  m_layers_container2D;

    CBVHCONTAINER2D*  m_platedpads_container2D_F_Cu;
    CBVHCONTAINER2D*  m_platedpads_container2D_B_Cu;

    /// It contains the holes per each layer
    MAP_CONTAINER_2D  m_layers_holes2D;

    /// It contains the list of throughHoles of the board,
    /// the radius of the hole is inflated with the copper tickness
    CBVHCONTAINER2D   m_through_holes_outer;

    /// It contains the list of throughHoles of the board,
    /// the radius of the hole is inflated with the annular ring size
    CBVHCONTAINER2D   m_through_holes_outer_ring;

    /// It contains the list of throughHoles of the board,
    /// the radius is the inner hole
    CBVHCONTAINER2D   m_through_holes_inner;

    /// It contains the list of throughHoles vias of the board,
    /// the radius of the hole is inflated with the copper tickness
    CBVHCONTAINER2D   m_through_holes_vias_outer;

    /// It contains the list of throughHoles vias of the board,
    /// the radius of the hole
    CBVHCONTAINER2D   m_through_holes_vias_inner;


    // Layers information

    /// Number of copper layers actually used by the board
    unsigned int m_copperLayersCount;

    /// Normalization scale to convert board internal units to 3D units to
    /// normalize 3D units between -1.0 and +1.0
    double m_biuTo3Dunits;

    /// Top (End) Z position of each layer (normalized)
    std::array<float, PCB_LAYER_ID_COUNT>  m_layerZcoordTop;

    /// Bottom (Start) Z position of each layer (normalized)
    std::array<float, PCB_LAYER_ID_COUNT>  m_layerZcoordBottom;

    /// Copper thickness (normalized)
    float  m_copperThickness3DU;

    /// Epoxy thickness (normalized)
    float  m_epoxyThickness3DU;

    /// Non copper layers thickness
    float  m_nonCopperLayerThickness3DU;

    /// min factor used for cicle segment approximation calculation
    float m_calc_seg_min_factor3DU;

    /// max factor used for cicle segment approximation calculation
    float m_calc_seg_max_factor3DU;


    // Statistics

    /// Number of tracks in the board
    unsigned int m_stats_nr_tracks;

    /// Track average width
    float        m_stats_track_med_width;

    /// Nr of vias
    unsigned int m_stats_nr_vias;

    /// Computed medium diameter of the via holes in 3D units
    float        m_stats_via_med_hole_diameter;

    /// number of holes in the board
    unsigned int m_stats_nr_holes;

    /// Computed medium diameter of the holes in 3D units
    float        m_stats_hole_med_diameter;

    /**
     *  Trace mask used to enable or disable the trace output of this class.
     *  The debug output can be turned on by setting the WXTRACE environment variable to
     *  "KI_TRACE_EDA_CINFO3D_VISU".  See the wxWidgets documentation on wxLogTrace for
     *  more information.
     */
    static const wxChar *m_logTrace;

};


class EDA_3D_BOARD_HOLDER
{
public:
    virtual BOARD_ADAPTER& GetAdapter() = 0;
    virtual CCAMERA&       GetCurrentCamera() = 0;

    virtual ~EDA_3D_BOARD_HOLDER() {};
};

#endif // BOARD_ADAPTER_H
