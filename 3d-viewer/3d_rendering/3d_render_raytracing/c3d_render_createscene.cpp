/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015-2016 Mario Luzeiro <mrluzeiro@ua.pt>
 * Copyright (C) 1992-2016 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file  c3d_render_createscene.cpp
 * @brief
 */


#include "c3d_render_raytracing.h"
#include "shapes3D/cplane.h"
#include "shapes3D/croundseg.h"
#include "shapes3D/clayeritem.h"
#include "shapes3D/ccylinder.h"
#include "shapes3D/ctriangle.h"
#include "shapes2D/citemlayercsg2d.h"
#include "shapes2D/cring2d.h"
#include "shapes2D/cpolygon2d.h"
#include "shapes2D/cfilledcircle2d.h"
#include "accelerators/cbvh_pbrt.h"
#include "3d_fastmath.h"
#include "3d_math.h"

#include <board.h>
#include <footprint.h>

#include <base_units.h>
#include <profile.h>        // To use GetRunningMicroSecs or another profiling utility

/**
 * @brief TransparencyAlphaControl
 * Perform an interpolation step to easy control the transparency based on the
 * gray color value and transparency
 * @param aGrayColorValue - diffuse gray value
 * @param aTransparency - control
 * @return transparency to use in material
 */
static float TransparencyControl( float aGrayColorValue, float aTransparency )
{
    const float aaa = aTransparency * aTransparency * aTransparency;

    // 1.00-1.05*(1.0-x)^3
    float ca = 1.0f - aTransparency;
    ca = 1.00f - 1.05f * ca * ca * ca;

    return glm::max( glm::min( aGrayColorValue * ca + aaa, 1.0f ), 0.0f );
}

/**
  * Scale convertion from 3d model units to pcb units
  */
#define UNITS3D_TO_UNITSPCB (IU_PER_MM)

void C3D_RENDER_RAYTRACING::setupMaterials()
{
    CMATERIAL::SetDefaultNrRefractionsSamples( m_boardAdapter.m_raytrace_nrsamples_refractions );
    CMATERIAL::SetDefaultNrReflectionsSamples( m_boardAdapter.m_raytrace_nrsamples_reflections );

    CMATERIAL::SetDefaultRefractionsLevel( m_boardAdapter.m_raytrace_recursivelevel_refractions );
    CMATERIAL::SetDefaultReflectionsLevel( m_boardAdapter.m_raytrace_recursivelevel_reflections );

    double mmTo3Dunits = IU_PER_MM * m_boardAdapter.BiuTo3Dunits();

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
    {
        m_board_normal_perturbator  = CBOARDNORMAL( 0.40f * mmTo3Dunits );

        m_copper_normal_perturbator = CCOPPERNORMAL( 4.0f * mmTo3Dunits,
                                                     &m_board_normal_perturbator );

        m_platedcopper_normal_perturbator = CPLATEDCOPPERNORMAL( 0.5f * mmTo3Dunits );

        m_solder_mask_normal_perturbator = CSOLDERMASKNORMAL( &m_board_normal_perturbator );

        m_plastic_normal_perturbator = CPLASTICNORMAL( 0.05f * mmTo3Dunits );

        m_plastic_shine_normal_perturbator = CPLASTICSHINENORMAL( 0.1f * mmTo3Dunits );

        m_brushed_metal_normal_perturbator = CMETALBRUSHEDNORMAL( 0.05f * mmTo3Dunits );

        m_silkscreen_normal_perturbator = CSILKSCREENNORMAL( 0.25f * mmTo3Dunits );
    }

    // http://devernay.free.fr/cours/opengl/materials.html

    // Copper
    const SFVEC3F copperSpecularLinear = ConvertSRGBToLinear(
                glm::clamp( (SFVEC3F)m_boardAdapter.m_CopperColor * 0.5f + 0.25f,
                            SFVEC3F( 0.0f ),
                            SFVEC3F( 1.0f ) ) );

    m_materials.m_Copper = CBLINN_PHONG_MATERIAL( ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_CopperColor * 0.3f ), // ambient
                                                  SFVEC3F( 0.0f ),           // emissive
                                                  copperSpecularLinear,      // specular
                                                  0.4f * 128.0f,             // shiness
                                                  0.0f,                      // transparency
                                                  0.0f );

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
        m_materials.m_Copper.SetNormalPerturbator( &m_platedcopper_normal_perturbator );

    m_materials.m_NonPlatedCopper = CBLINN_PHONG_MATERIAL(
                    ConvertSRGBToLinear( SFVEC3F( 0.191f, 0.073f, 0.022f ) ),// ambient
                    SFVEC3F( 0.0f, 0.0f, 0.0f ),                             // emissive
                    SFVEC3F( 0.256f, 0.137f, 0.086f ),                       // specular
                    0.15f * 128.0f,                                          // shiness
                    0.0f,                                                    // transparency
                    0.0f );

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
        m_materials.m_NonPlatedCopper.SetNormalPerturbator( &m_copper_normal_perturbator );

    m_materials.m_Paste = CBLINN_PHONG_MATERIAL(
            ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_SolderPasteColor ) *
            ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_SolderPasteColor ), // ambient
                SFVEC3F( 0.0f, 0.0f, 0.0f ),            // emissive
                ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_SolderPasteColor ) *
                ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_SolderPasteColor ), // specular
                0.10f * 128.0f,                         // shiness
                0.0f,                                   // transparency
                0.0f );

    m_materials.m_SilkS = CBLINN_PHONG_MATERIAL( ConvertSRGBToLinear( SFVEC3F( 0.11f ) ), // ambient
            SFVEC3F( 0.0f, 0.0f, 0.0f ), // emissive
            glm::clamp(
                    ( ( SFVEC3F )( 1.0f )
                            - ConvertSRGBToLinear( (SFVEC3F) m_boardAdapter.m_SilkScreenColorTop ) ),
                    SFVEC3F( 0.0f ),
                    SFVEC3F( 0.10f ) ), // specular
            0.078125f * 128.0f,         // shiness
            0.0f,                       // transparency
            0.0f );

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
        m_materials.m_SilkS.SetNormalPerturbator( &m_silkscreen_normal_perturbator );

    // Assume that SolderMaskTop == SolderMaskBot
    const float solderMask_gray =
            ( m_boardAdapter.m_SolderMaskColorTop.r + m_boardAdapter.m_SolderMaskColorTop.g
              + m_boardAdapter.m_SolderMaskColorTop.b )
            / 3.0f;

    const float solderMask_transparency = TransparencyControl( solderMask_gray,
                                                               1.0f - m_boardAdapter.m_SolderMaskColorTop.a ); // opacity to transparency

    m_materials.m_SolderMask = CBLINN_PHONG_MATERIAL(
            ConvertSRGBToLinear( (SFVEC3F) m_boardAdapter.m_SolderMaskColorTop ) * 0.10f, // ambient
            SFVEC3F( 0.0f, 0.0f, 0.0f ),                                              // emissive
            SFVEC3F( glm::clamp( solderMask_gray * 2.0f, 0.25f, 1.0f ) ),             // specular
            0.85f * 128.0f,                                 // shiness
            solderMask_transparency,                        // transparency
            0.16f );                                        // reflection

    m_materials.m_SolderMask.SetCastShadows( true );
    m_materials.m_SolderMask.SetNrRefractionsSamples( 1 );

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
        m_materials.m_SolderMask.SetNormalPerturbator( &m_solder_mask_normal_perturbator );

    m_materials.m_EpoxyBoard = CBLINN_PHONG_MATERIAL(
                ConvertSRGBToLinear( SFVEC3F( 16.0f / 255.0f,
                                              14.0f / 255.0f,
                                              10.0f / 255.0f ) ), // ambient
                SFVEC3F( 0.0f, 0.0f, 0.0f ),                      // emissive
                ConvertSRGBToLinear( SFVEC3F( 10.0f / 255.0f,
                                              8.0f / 255.0f,
                                              10.0f / 255.0f ) ), // specular
                0.1f * 128.0f,                                    // shiness
                1.0f - m_boardAdapter.m_BoardBodyColor.a,         // opacity to transparency
                0.0f );                                           // reflection

    m_materials.m_EpoxyBoard.SetAbsorvance( 10.0f );

    if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
        m_materials.m_EpoxyBoard.SetNormalPerturbator( &m_board_normal_perturbator );

    SFVEC3F bgTop = ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_BgColorTop );
    //SFVEC3F bgBot = (SFVEC3F)m_boardAdapter.m_BgColorBot;

    m_materials.m_Floor = CBLINN_PHONG_MATERIAL(
                bgTop * 0.125f,                         // ambient
                SFVEC3F( 0.0f, 0.0f, 0.0f ),            // emissive
                (SFVEC3F(1.0f) - bgTop) / 3.0f,         // specular
                0.10f * 128.0f,                         // shiness
                0.0f,                                   // transparency
                0.50f );                                // reflection
    m_materials.m_Floor.SetCastShadows( false );
    m_materials.m_Floor.SetReflectionsRecursiveLevel( 1 );
}



/** Function create_3d_object_from
 * @brief Creates on or more 3D objects form a 2D object and Z positions. It try
 * optimize some types of objects that will be faster to trace than the
 * CLAYERITEM object.
 * @param aObject2D
 * @param aZMin
 * @param aZMax
 */
void C3D_RENDER_RAYTRACING::create_3d_object_from( CCONTAINER& aDstContainer,
        const COBJECT2D* aObject2D, float aZMin, float aZMax, const CMATERIAL* aMaterial,
        const SFVEC3F& aObjColor )
{
    switch( aObject2D->GetObjectType() )
    {
    case OBJECT2D_TYPE::DUMMYBLOCK:
    {
        m_stats_converted_dummy_to_plane++;
#if 1
        CXYPLANE* objPtr;
        objPtr = new CXYPLANE( CBBOX(
                SFVEC3F( aObject2D->GetBBox().Min().x, aObject2D->GetBBox().Min().y, aZMin ),
                SFVEC3F( aObject2D->GetBBox().Max().x, aObject2D->GetBBox().Max().y, aZMin ) ) );
        objPtr->SetMaterial( aMaterial );
        objPtr->SetColor( ConvertSRGBToLinear( aObjColor ) );
        aDstContainer.Add( objPtr );

        objPtr = new CXYPLANE( CBBOX(
                SFVEC3F( aObject2D->GetBBox().Min().x, aObject2D->GetBBox().Min().y, aZMax ),
                SFVEC3F( aObject2D->GetBBox().Max().x, aObject2D->GetBBox().Max().y, aZMax ) ) );
        objPtr->SetMaterial( aMaterial );
        objPtr->SetColor( ConvertSRGBToLinear( aObjColor ) );
        aDstContainer.Add( objPtr );
#else
        objPtr = new CDUMMYBLOCK( CBBOX(
                SFVEC3F( aObject2D->GetBBox().Min().x, aObject2D->GetBBox().Min().y, aZMin ),
                SFVEC3F( aObject2D->GetBBox().Max().x, aObject2D->GetBBox().Max().y, aZMax ) ) );
        objPtr->SetMaterial( aMaterial );
        aDstContainer.Add( objPtr );
#endif
    }
    break;

    case OBJECT2D_TYPE::ROUNDSEG:
    {
        m_stats_converted_roundsegment2d_to_roundsegment++;

        const CROUNDSEGMENT2D* aRoundSeg2D = static_cast<const CROUNDSEGMENT2D*>( aObject2D );
        CROUNDSEG*             objPtr      = new CROUNDSEG( *aRoundSeg2D, aZMin, aZMax );
        objPtr->SetMaterial( aMaterial );
        objPtr->SetColor( ConvertSRGBToLinear( aObjColor ) );
        aDstContainer.Add( objPtr );
    }
    break;


    default:
    {
        CLAYERITEM* objPtr = new CLAYERITEM( aObject2D, aZMin, aZMax );
        objPtr->SetMaterial( aMaterial );
        objPtr->SetColor( ConvertSRGBToLinear( aObjColor ) );
        aDstContainer.Add( objPtr );
    }
    break;
    }
}

void C3D_RENDER_RAYTRACING::createItemsFromContainer( const CBVHCONTAINER2D *aContainer2d,
                                                      PCB_LAYER_ID aLayer_id,
                                                      const CMATERIAL *aMaterialLayer,
                                                      const SFVEC3F &aLayerColor,
                                                      float aLayerZOffset )
{
    if( aContainer2d == nullptr )
        return;

    const LIST_OBJECT2D &listObject2d = aContainer2d->GetList();

    if( listObject2d.size() == 0 )
        return;

    for( LIST_OBJECT2D::const_iterator itemOnLayer = listObject2d.begin();
         itemOnLayer != listObject2d.end();
         ++itemOnLayer )
    {
        const COBJECT2D *object2d_A = static_cast<const COBJECT2D *>(*itemOnLayer);

        // not yet used / implemented (can be used in future to clip the objects in the board borders
        COBJECT2D *object2d_C = CSGITEM_FULL;

        std::vector<const COBJECT2D *> *object2d_B = CSGITEM_EMPTY;

        object2d_B = new std::vector<const COBJECT2D*>();

        // Subtract holes but not in SolderPaste
        // (can be added as an option in future)
        if( !( ( aLayer_id == B_Paste ) || ( aLayer_id == F_Paste ) ) )
        {
            // Check if there are any layerhole that intersects this object
            // Eg: a segment is cutted by a via hole or THT hole.
            // /////////////////////////////////////////////////////////////
            const MAP_CONTAINER_2D &layerHolesMap = m_boardAdapter.GetMapLayersHoles();

            if( layerHolesMap.find(aLayer_id) != layerHolesMap.end() )
            {
                MAP_CONTAINER_2D::const_iterator ii_hole = layerHolesMap.find(aLayer_id);

                const CBVHCONTAINER2D *containerLayerHoles2d =
                        static_cast<const CBVHCONTAINER2D *>(ii_hole->second);

                CONST_LIST_OBJECT2D intersectionList;
                containerLayerHoles2d->GetListObjectsIntersects( object2d_A->GetBBox(),
                                                                 intersectionList );

                if( !intersectionList.empty() )
                {
                    for( CONST_LIST_OBJECT2D::const_iterator holeOnLayer =
                         intersectionList.begin();
                         holeOnLayer != intersectionList.end();
                         ++holeOnLayer )
                    {
                        const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*holeOnLayer);

                        //if( object2d_A->Intersects( hole2d->GetBBox() ) )
                            //if( object2d_A->GetBBox().Intersects( hole2d->GetBBox() ) )
                                object2d_B->push_back( hole2d );
                    }
                }
            }

            // Check if there are any THT that intersects this object
            // /////////////////////////////////////////////////////////////

            // If we're processing a silk screen layer and the flag is set, then
            // clip the silk screening at the outer edge of the annular ring, rather
            // than the at the outer edge of the copper plating.
            const CBVHCONTAINER2D& throughHoleOuter =
                    ( m_boardAdapter.GetFlag( FL_CLIP_SILK_ON_VIA_ANNULUS ) &&
                      m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE )
                            && ( ( aLayer_id == B_SilkS ) || ( aLayer_id == F_SilkS ) ) ) ?
                            m_boardAdapter.GetThroughHole_Outer_Ring() :
                            m_boardAdapter.GetThroughHole_Outer();

            if( !throughHoleOuter.GetList().empty() )
            {
                CONST_LIST_OBJECT2D intersectionList;

                throughHoleOuter.GetListObjectsIntersects(
                        object2d_A->GetBBox(), intersectionList );

                if( !intersectionList.empty() )
                {
                    for( CONST_LIST_OBJECT2D::const_iterator hole = intersectionList.begin();
                         hole != intersectionList.end();
                         ++hole )
                    {
                        const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*hole);

                        //if( object2d_A->Intersects( hole2d->GetBBox() ) )
                            //if( object2d_A->GetBBox().Intersects( hole2d->GetBBox() ) )
                                object2d_B->push_back( hole2d );
                    }
                }
            }
        }

        if( !m_antioutlineBoard2dObjects->GetList().empty() )
        {
            CONST_LIST_OBJECT2D intersectionList;

            m_antioutlineBoard2dObjects->GetListObjectsIntersects(
                        object2d_A->GetBBox(), intersectionList );

            if( !intersectionList.empty() )
            {
                for( const COBJECT2D *obj : intersectionList )
                {
                    object2d_B->push_back( obj );
                }
            }
        }

        const MAP_CONTAINER_2D& mapLayers = m_boardAdapter.GetMapLayers();

        if( m_boardAdapter.GetFlag( FL_SUBTRACT_MASK_FROM_SILK ) &&
            m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) &&
            ( ( ( aLayer_id == B_SilkS ) &&
                ( mapLayers.find( B_Mask ) != mapLayers.end() ) ) ||
              ( ( aLayer_id == F_SilkS ) &&
                ( mapLayers.find( F_Mask ) != mapLayers.end() ) ) ) )
        {
            const PCB_LAYER_ID layerMask_id = ( aLayer_id == B_SilkS ) ? B_Mask : F_Mask;

            const CBVHCONTAINER2D *containerMaskLayer2d =
                    static_cast<const CBVHCONTAINER2D*>( mapLayers.at( layerMask_id ) );

            CONST_LIST_OBJECT2D intersectionList;

            if( containerMaskLayer2d )  // can be null if B_Mask or F_Mask is not shown
                containerMaskLayer2d->GetListObjectsIntersects( object2d_A->GetBBox(),
                                                                intersectionList );

            if( !intersectionList.empty() )
            {
                for( CONST_LIST_OBJECT2D::const_iterator objOnLayer =
                     intersectionList.begin();
                     objOnLayer != intersectionList.end();
                     ++objOnLayer )
                {
                    const COBJECT2D* obj2d = static_cast<const COBJECT2D*>( *objOnLayer );

                    object2d_B->push_back( obj2d );
                }
            }
        }

        if( object2d_B->empty() )
        {
            delete object2d_B;
            object2d_B = CSGITEM_EMPTY;
        }

        if( (object2d_B == CSGITEM_EMPTY) &&
            (object2d_C == CSGITEM_FULL) )
        {
            CLAYERITEM *objPtr = new CLAYERITEM( object2d_A,
                                                 m_boardAdapter.GetLayerBottomZpos3DU( aLayer_id ) - aLayerZOffset,
                                                 m_boardAdapter.GetLayerTopZpos3DU( aLayer_id ) + aLayerZOffset );
            objPtr->SetMaterial( aMaterialLayer );
            objPtr->SetColor( ConvertSRGBToLinear( aLayerColor ) );
            m_object_container.Add( objPtr );
        }
        else
        {
            CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D( object2d_A,
                                                              object2d_B,
                                                              object2d_C,
                                                              object2d_A->GetBoardItem() );
            m_containerWithObjectsToDelete.Add( itemCSG2d );

            CLAYERITEM *objPtr = new CLAYERITEM( itemCSG2d,
                                                 m_boardAdapter.GetLayerBottomZpos3DU( aLayer_id ) - aLayerZOffset,
                                                 m_boardAdapter.GetLayerTopZpos3DU( aLayer_id ) + aLayerZOffset );

            objPtr->SetMaterial( aMaterialLayer );
            objPtr->SetColor( ConvertSRGBToLinear( aLayerColor ) );

            m_object_container.Add( objPtr );
        }
    }
}

extern void buildBoardBoundingBoxPoly( const BOARD* aBoard, SHAPE_POLY_SET& aOutline );

void C3D_RENDER_RAYTRACING::Reload( REPORTER* aStatusReporter,
                                    REPORTER* aWarningReporter,
                                    bool aOnlyLoadCopperAndShapes )
{
    m_reloadRequested = false;

    m_model_materials.clear();

    COBJECT2D_STATS::Instance().ResetStats();
    COBJECT3D_STATS::Instance().ResetStats();

    unsigned stats_startReloadTime = GetRunningMicroSecs();

    if( !aOnlyLoadCopperAndShapes )
    {
        m_boardAdapter.InitSettings( aStatusReporter, aWarningReporter );

        SFVEC3F camera_pos = m_boardAdapter.GetBoardCenter3DU();
        m_camera.SetBoardLookAtPos( camera_pos );
    }

    m_object_container.Clear();
    m_containerWithObjectsToDelete.Clear();

    setupMaterials();

    if( aStatusReporter )
        aStatusReporter->Report( _( "Load Raytracing: board" ) );

    // Create and add the outline board
    // /////////////////////////////////////////////////////////////////////////

    delete m_outlineBoard2dObjects;
    delete m_antioutlineBoard2dObjects;

    m_outlineBoard2dObjects = new CCONTAINER2D;
    m_antioutlineBoard2dObjects = new CBVHCONTAINER2D;

    if( !aOnlyLoadCopperAndShapes )
    {
        const int outlineCount = m_boardAdapter.GetBoardPoly().OutlineCount();

        if( outlineCount > 0 )
        {
            float divFactor = 0.0f;

            if( m_boardAdapter.GetStats_Nr_Vias() )
                divFactor = m_boardAdapter.GetStats_Med_Via_Hole_Diameter3DU() * 18.0f;
            else
                if( m_boardAdapter.GetStats_Nr_Holes() )
                    divFactor = m_boardAdapter.GetStats_Med_Hole_Diameter3DU() * 8.0f;

            SHAPE_POLY_SET boardPolyCopy = m_boardAdapter.GetBoardPoly();

            // Calculate an antiboard outline

            SHAPE_POLY_SET antiboardPoly;

            buildBoardBoundingBoxPoly( m_boardAdapter.GetBoard(), antiboardPoly );

            antiboardPoly.BooleanSubtract( boardPolyCopy, SHAPE_POLY_SET::PM_FAST );
            antiboardPoly.Fracture( SHAPE_POLY_SET::PM_FAST );

            for( int iOutlinePolyIdx = 0; iOutlinePolyIdx < antiboardPoly.OutlineCount(); iOutlinePolyIdx++ )
            {
                Convert_path_polygon_to_polygon_blocks_and_dummy_blocks(
                        antiboardPoly,
                        *m_antioutlineBoard2dObjects,
                        m_boardAdapter.BiuTo3Dunits(),
                        -1.0f,
                        *dynamic_cast<const BOARD_ITEM*>( m_boardAdapter.GetBoard() ),
                        iOutlinePolyIdx );
            }

            m_antioutlineBoard2dObjects->BuildBVH();

            boardPolyCopy.Fracture( SHAPE_POLY_SET::PM_FAST );

            for( int iOutlinePolyIdx = 0; iOutlinePolyIdx < outlineCount; iOutlinePolyIdx++ )
            {
                Convert_path_polygon_to_polygon_blocks_and_dummy_blocks(
                        boardPolyCopy,
                        *m_outlineBoard2dObjects,
                        m_boardAdapter.BiuTo3Dunits(),
                        divFactor,
                        *dynamic_cast<const BOARD_ITEM*>( m_boardAdapter.GetBoard() ),
                        iOutlinePolyIdx );
            }

            if( m_boardAdapter.GetFlag( FL_SHOW_BOARD_BODY ) )
            {
                const LIST_OBJECT2D &listObjects = m_outlineBoard2dObjects->GetList();

                for( LIST_OBJECT2D::const_iterator object2d_iterator = listObjects.begin();
                     object2d_iterator != listObjects.end();
                     ++object2d_iterator )
                {
                    const COBJECT2D *object2d_A = static_cast<const COBJECT2D *>(*object2d_iterator);

                    std::vector<const COBJECT2D *> *object2d_B = new std::vector<const COBJECT2D *>();

                    // Check if there are any THT that intersects this outline object part
                    if( !m_boardAdapter.GetThroughHole_Outer().GetList().empty() )
                    {

                        CONST_LIST_OBJECT2D intersectionList;
                        m_boardAdapter.GetThroughHole_Outer().GetListObjectsIntersects(
                                    object2d_A->GetBBox(),
                                    intersectionList );

                        if( !intersectionList.empty() )
                        {
                            for( CONST_LIST_OBJECT2D::const_iterator hole = intersectionList.begin();
                                 hole != intersectionList.end();
                                 ++hole )
                            {
                                const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*hole);

                                if( object2d_A->Intersects( hole2d->GetBBox() ) )
                                //if( object2d_A->GetBBox().Intersects( hole2d->GetBBox() ) )
                                    object2d_B->push_back( hole2d );
                            }
                        }
                    }

                    if( !m_antioutlineBoard2dObjects->GetList().empty() )
                    {
                        CONST_LIST_OBJECT2D intersectionList;

                        m_antioutlineBoard2dObjects->GetListObjectsIntersects(
                                    object2d_A->GetBBox(),
                                    intersectionList );

                        if( !intersectionList.empty() )
                        {
                            for( const COBJECT2D *obj : intersectionList )
                            {
                                object2d_B->push_back( obj );
                            }
                        }
                    }

                    if( object2d_B->empty() )
                    {
                        delete object2d_B;
                        object2d_B = CSGITEM_EMPTY;
                    }

                    if( object2d_B == CSGITEM_EMPTY )
                    {
                        CLAYERITEM *objPtr = new CLAYERITEM( object2d_A,
                                                             m_boardAdapter.GetLayerBottomZpos3DU( F_Cu ),
                                                             m_boardAdapter.GetLayerBottomZpos3DU( B_Cu ) );

                        objPtr->SetMaterial( &m_materials.m_EpoxyBoard );
                        objPtr->SetColor( ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_BoardBodyColor ) );
                        m_object_container.Add( objPtr );
                    }
                    else
                    {

                        CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D(
                                    object2d_A,
                                    object2d_B,
                                    CSGITEM_FULL,
                                    (const BOARD_ITEM &)*m_boardAdapter.GetBoard() );

                        m_containerWithObjectsToDelete.Add( itemCSG2d );

                        CLAYERITEM *objPtr = new CLAYERITEM( itemCSG2d,
                                                             m_boardAdapter.GetLayerBottomZpos3DU( F_Cu ),
                                                             m_boardAdapter.GetLayerBottomZpos3DU( B_Cu ) );

                        objPtr->SetMaterial( &m_materials.m_EpoxyBoard );
                        objPtr->SetColor( ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_BoardBodyColor ) );
                        m_object_container.Add( objPtr );

                    }
                }

                // Add cylinders of the board body to container
                // Note: This is actually a workarround for the holes in the board.
                // The issue is because if a hole is in a border of a divided polygon ( ex
                // a polygon or dummyblock) it will cut also the render of the hole.
                // So this will add a full hole.
                // In fact, that is not need if the hole have copper.
                // /////////////////////////////////////////////////////////////////////////
                if( !m_boardAdapter.GetThroughHole_Outer().GetList().empty() )
                {
                    const LIST_OBJECT2D &holeList = m_boardAdapter.GetThroughHole_Outer().GetList();

                    for( LIST_OBJECT2D::const_iterator hole = holeList.begin();
                         hole != holeList.end();
                         ++hole )
                    {
                        const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*hole);

                        if( !m_antioutlineBoard2dObjects->GetList().empty() )
                        {
                            CONST_LIST_OBJECT2D intersectionList;

                            m_antioutlineBoard2dObjects->GetListObjectsIntersects(
                                        hole2d->GetBBox(), intersectionList );

                            if( !intersectionList.empty() )
                            {
                                // Do not add cylinder if it intersects the edge of the board

                                continue;
                            }
                        }

                        switch( hole2d->GetObjectType() )
                        {
                        case OBJECT2D_TYPE::FILLED_CIRCLE:
                        {
                            const float radius = hole2d->GetBBox().GetExtent().x * 0.5f * 0.999f;

                            CVCYLINDER *objPtr = new CVCYLINDER(
                                        hole2d->GetCentroid(),
                                        NextFloatDown( m_boardAdapter.GetLayerBottomZpos3DU( F_Cu ) ),
                                        NextFloatUp( m_boardAdapter.GetLayerBottomZpos3DU( B_Cu ) ),
                                        radius );

                            objPtr->SetMaterial( &m_materials.m_EpoxyBoard );
                            objPtr->SetColor( ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_BoardBodyColor ) );

                            m_object_container.Add( objPtr );
                        }
                        break;

                        default:
                            break;
                        }
                    }
                }
            }
        }
    }

    if( aStatusReporter )
        aStatusReporter->Report( _( "Load Raytracing: layers" ) );

    // Add layers maps (except B_Mask and F_Mask)
    // /////////////////////////////////////////////////////////////////////////

    for( MAP_CONTAINER_2D::const_iterator ii = m_boardAdapter.GetMapLayers().begin();
         ii != m_boardAdapter.GetMapLayers().end();
         ++ii )
    {
        PCB_LAYER_ID layer_id = static_cast<PCB_LAYER_ID>(ii->first);

        if( aOnlyLoadCopperAndShapes && !IsCopperLayer( layer_id ) )
            continue;

        // Mask layers are not processed here because they are a special case
        if( (layer_id == B_Mask) || (layer_id == F_Mask) )
            continue;

        CMATERIAL *materialLayer = &m_materials.m_SilkS;
        SFVEC3F layerColor = SFVEC3F( 0.0f, 0.0f, 0.0f );

        switch( layer_id )
        {
            case B_Adhes:
            case F_Adhes:
            break;

            case B_Paste:
            case F_Paste:
                materialLayer = &m_materials.m_Paste;

                if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
                    layerColor = m_boardAdapter.m_SolderPasteColor;
                else
                    layerColor = m_boardAdapter.GetLayerColor( layer_id );
            break;

            case B_SilkS:
                materialLayer = &m_materials.m_SilkS;

                if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
                    layerColor = m_boardAdapter.m_SilkScreenColorBot;
                else
                    layerColor = m_boardAdapter.GetLayerColor( layer_id );
                break;
            case F_SilkS:
                materialLayer = &m_materials.m_SilkS;

                if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
                    layerColor = m_boardAdapter.m_SilkScreenColorTop;
                else
                    layerColor = m_boardAdapter.GetLayerColor( layer_id );
            break;

            case Dwgs_User:
            case Cmts_User:
            case Eco1_User:
            case Eco2_User:
            case Edge_Cuts:
            case Margin:
            break;

            case B_CrtYd:
            case F_CrtYd:
            break;

            case B_Fab:
            case F_Fab:
            break;

            default:
                if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
                {
                    if( m_boardAdapter.GetFlag( FL_RENDER_PLATED_PADS_AS_PLATED ) )
                        layerColor = SFVEC3F( 184.0f / 255.0f, 115.0f / 255.0f, 50.0f / 255.0f );
                    else
                        layerColor = m_boardAdapter.m_CopperColor;
                }
                else
                    layerColor = m_boardAdapter.GetLayerColor( layer_id );

                materialLayer = &m_materials.m_NonPlatedCopper;
            break;
        }

        const CBVHCONTAINER2D* container2d = static_cast<const CBVHCONTAINER2D*>(ii->second);

        createItemsFromContainer( container2d, layer_id, materialLayer, layerColor, 0.0f );
    }// for each layer on map

    // Create plated copper
    if( m_boardAdapter.GetFlag( FL_RENDER_PLATED_PADS_AS_PLATED ) &&
        m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
    {
        createItemsFromContainer( m_boardAdapter.GetPlatedPads_Front(), F_Cu, &m_materials.m_Copper,
                                  m_boardAdapter.m_CopperColor, +m_boardAdapter.GetCopperThickness3DU() * 0.1f );

        createItemsFromContainer( m_boardAdapter.GetPlatedPads_Back(), B_Cu, &m_materials.m_Copper,
                                  m_boardAdapter.m_CopperColor, -m_boardAdapter.GetCopperThickness3DU() * 0.1f );
    }

    if( !aOnlyLoadCopperAndShapes )
    {
        // Add Mask layer
        // Solder mask layers are "negative" layers so the elements that we have
        // (in the container) should remove the board outline.
        // We will check for all objects in the outline if it intersects any object
        // in the layer container and also any hole.
        // /////////////////////////////////////////////////////////////////////////
        if( m_boardAdapter.GetFlag( FL_SOLDERMASK ) &&
            (m_outlineBoard2dObjects->GetList().size() >= 1) )
        {
            const CMATERIAL *materialLayer = &m_materials.m_SolderMask;

            for( MAP_CONTAINER_2D::const_iterator ii = m_boardAdapter.GetMapLayers().begin();
                 ii != m_boardAdapter.GetMapLayers().end();
                 ++ii )
            {
                PCB_LAYER_ID layer_id = static_cast<PCB_LAYER_ID>(ii->first);

                const CBVHCONTAINER2D *containerLayer2d =
                        static_cast<const CBVHCONTAINER2D *>(ii->second);

                // Only get the Solder mask layers
                if( !( layer_id == B_Mask || layer_id == F_Mask ) )
                    continue;

                SFVEC3F layerColor;
                if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
                {
                    if( layer_id == B_Mask )
                        layerColor = m_boardAdapter.m_SolderMaskColorBot;
                    else
                        layerColor = m_boardAdapter.m_SolderMaskColorTop;
                }
                else
                    layerColor = m_boardAdapter.GetLayerColor( layer_id );

                const float zLayerMin = m_boardAdapter.GetLayerBottomZpos3DU( layer_id );
                const float zLayerMax = m_boardAdapter.GetLayerTopZpos3DU( layer_id );

                // Get the outline board objects
                const LIST_OBJECT2D &listObjects = m_outlineBoard2dObjects->GetList();

                for( LIST_OBJECT2D::const_iterator object2d_iterator = listObjects.begin();
                     object2d_iterator != listObjects.end();
                     ++object2d_iterator )
                {
                    const COBJECT2D *object2d_A = static_cast<const COBJECT2D *>(*object2d_iterator);

                    std::vector<const COBJECT2D *> *object2d_B = new std::vector<const COBJECT2D *>();

                    // Check if there are any THT that intersects this outline object part
                    if( !m_boardAdapter.GetThroughHole_Outer().GetList().empty() )
                    {

                        CONST_LIST_OBJECT2D intersectionList;

                        m_boardAdapter.GetThroughHole_Outer().GetListObjectsIntersects(
                                    object2d_A->GetBBox(),
                                    intersectionList );

                        if( !intersectionList.empty() )
                        {
                            for( CONST_LIST_OBJECT2D::const_iterator hole = intersectionList.begin();
                                 hole != intersectionList.end();
                                 ++hole )
                            {
                                const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*hole);

                                if( object2d_A->Intersects( hole2d->GetBBox() ) )
                                //if( object2d_A->GetBBox().Intersects( hole2d->GetBBox() ) )
                                    object2d_B->push_back( hole2d );
                            }
                        }
                    }

                    // Check if there are any objects in the layer to subtract with the
                    // corrent object
                    if( !containerLayer2d->GetList().empty() )
                    {
                        CONST_LIST_OBJECT2D intersectionList;

                        containerLayer2d->GetListObjectsIntersects( object2d_A->GetBBox(),
                                                                    intersectionList );

                        if( !intersectionList.empty() )
                        {
                            for( CONST_LIST_OBJECT2D::const_iterator obj = intersectionList.begin();
                                 obj != intersectionList.end();
                                 ++obj )
                            {
                                const COBJECT2D *obj2d = static_cast<const COBJECT2D *>(*obj);

                                //if( object2d_A->Intersects( obj2d->GetBBox() ) )
                                //if( object2d_A->GetBBox().Intersects( obj2d->GetBBox() ) )
                                    object2d_B->push_back( obj2d );
                            }
                        }
                    }

                    if( object2d_B->empty() )
                    {
                        delete object2d_B;
                        object2d_B = CSGITEM_EMPTY;
                    }

                    if( object2d_B == CSGITEM_EMPTY )
                    {
            #if 0
                       create_3d_object_from( m_object_container,
                                              object2d_A,
                                              zLayerMin,
                                              zLayerMax,
                                              materialLayer,
                                              layerColor );
            #else
                        CLAYERITEM *objPtr =  new CLAYERITEM( object2d_A,
                                                              zLayerMin,
                                                              zLayerMax );

                        objPtr->SetMaterial( materialLayer );
                        objPtr->SetColor( ConvertSRGBToLinear( layerColor ) );

                        m_object_container.Add( objPtr );
            #endif
                    }
                    else
                    {
                        CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D( object2d_A,
                                                                          object2d_B,
                                                                          CSGITEM_FULL,
                                                                          object2d_A->GetBoardItem() );

                        m_containerWithObjectsToDelete.Add( itemCSG2d );

                        CLAYERITEM *objPtr =  new CLAYERITEM( itemCSG2d,
                                                              zLayerMin,
                                                              zLayerMax );
                        objPtr->SetMaterial( materialLayer );
                        objPtr->SetColor( ConvertSRGBToLinear( layerColor ) );

                        m_object_container.Add( objPtr );
                    }
                }
            }
        }

        add_3D_vias_and_pads_to_container();
    }

#ifdef PRINT_STATISTICS_3D_VIEWER
    unsigned stats_endConvertTime = GetRunningMicroSecs();
    unsigned stats_startLoad3DmodelsTime = stats_endConvertTime;
#endif

    if( aStatusReporter )
        aStatusReporter->Report( _( "Loading 3D models" ) );

    load_3D_models( m_object_container, aOnlyLoadCopperAndShapes );


#ifdef PRINT_STATISTICS_3D_VIEWER
    unsigned stats_endLoad3DmodelsTime = GetRunningMicroSecs();
#endif

    if( !aOnlyLoadCopperAndShapes )
    {
        // Add floor
        // /////////////////////////////////////////////////////////////////////////
        if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_BACKFLOOR ) )
        {
            CBBOX boardBBox = m_boardAdapter.GetBBox3DU();

            if( boardBBox.IsInitialized() )
            {
                boardBBox.Scale( 3.0f );

                if( m_object_container.GetList().size() > 0 )
                {
                    CBBOX containerBBox = m_object_container.GetBBox();

                    containerBBox.Scale( 1.3f );

                    const SFVEC3F centerBBox = containerBBox.GetCenter();

                    // Floor triangles
                    const float minZ = glm::min( containerBBox.Min().z,
                                                 boardBBox.Min().z );

                    const SFVEC3F v1 = SFVEC3F( -RANGE_SCALE_3D * 4.0f,
                                                -RANGE_SCALE_3D * 4.0f,
                                                minZ ) +
                                       SFVEC3F( centerBBox.x,
                                                centerBBox.y,
                                                0.0f );

                    const SFVEC3F v3 = SFVEC3F( +RANGE_SCALE_3D * 4.0f,
                                                +RANGE_SCALE_3D * 4.0f,
                                                minZ ) +
                                       SFVEC3F( centerBBox.x,
                                                centerBBox.y,
                                                0.0f );

                    const SFVEC3F v2 = SFVEC3F( v1.x, v3.y, v1.z );
                    const SFVEC3F v4 = SFVEC3F( v3.x, v1.y, v1.z );

                    SFVEC3F backgroundColor =
                            ConvertSRGBToLinear( static_cast<SFVEC3F>( m_boardAdapter.m_BgColorTop ) );

                    CTRIANGLE *newTriangle1 = new CTRIANGLE( v1, v2, v3 );
                    CTRIANGLE *newTriangle2 = new CTRIANGLE( v3, v4, v1 );

                    m_object_container.Add( newTriangle1 );
                    m_object_container.Add( newTriangle2 );

                    newTriangle1->SetMaterial( (const CMATERIAL *)&m_materials.m_Floor );
                    newTriangle2->SetMaterial( (const CMATERIAL *)&m_materials.m_Floor );

                    newTriangle1->SetColor( backgroundColor );
                    newTriangle2->SetColor( backgroundColor );

                    // Ceiling triangles
                    const float maxZ = glm::max( containerBBox.Max().z,
                                                 boardBBox.Max().z );

                    const SFVEC3F v5 = SFVEC3F( v1.x, v1.y, maxZ );
                    const SFVEC3F v6 = SFVEC3F( v2.x, v2.y, maxZ );
                    const SFVEC3F v7 = SFVEC3F( v3.x, v3.y, maxZ );
                    const SFVEC3F v8 = SFVEC3F( v4.x, v4.y, maxZ );

                    CTRIANGLE *newTriangle3 = new CTRIANGLE( v7, v6, v5 );
                    CTRIANGLE *newTriangle4 = new CTRIANGLE( v5, v8, v7 );

                    m_object_container.Add( newTriangle3 );
                    m_object_container.Add( newTriangle4 );

                    newTriangle3->SetMaterial( (const CMATERIAL *)&m_materials.m_Floor );
                    newTriangle4->SetMaterial( (const CMATERIAL *)&m_materials.m_Floor );

                    newTriangle3->SetColor( backgroundColor );
                    newTriangle4->SetColor( backgroundColor );
                }
            }
        }


        // Init initial lights
        // /////////////////////////////////////////////////////////////////////////
        m_lights.Clear();

        auto IsColorZero = [] ( const SFVEC3F& aSource )
        {
            return ( ( aSource.r < ( 1.0f / 255.0f ) ) &&
                     ( aSource.g < ( 1.0f / 255.0f ) ) &&
                     ( aSource.b < ( 1.0f / 255.0f ) ) );
        };

        m_camera_light = new CDIRECTIONALLIGHT( SFVEC3F( 0.0f, 0.0f, 0.0f ),
                                                m_boardAdapter.m_raytrace_lightColorCamera );
        m_camera_light->SetCastShadows( false );

        if( !IsColorZero( m_boardAdapter.m_raytrace_lightColorCamera ) )
            m_lights.Add( m_camera_light );

        const SFVEC3F& boardCenter = m_boardAdapter.GetBBox3DU().GetCenter();

        if( !IsColorZero( m_boardAdapter.m_raytrace_lightColorTop ) )
            m_lights.Add( new CPOINTLIGHT( SFVEC3F( boardCenter.x, boardCenter.y, +RANGE_SCALE_3D * 2.0f ),
                                           m_boardAdapter.m_raytrace_lightColorTop ) );

        if( !IsColorZero( m_boardAdapter.m_raytrace_lightColorBottom ) )
            m_lights.Add( new CPOINTLIGHT( SFVEC3F( boardCenter.x, boardCenter.y, -RANGE_SCALE_3D * 2.0f ),
                                           m_boardAdapter.m_raytrace_lightColorBottom ) );

        wxASSERT( m_boardAdapter.m_raytrace_lightColor.size()
                  == m_boardAdapter.m_raytrace_lightSphericalCoords.size() );

        for( size_t i = 0; i < m_boardAdapter.m_raytrace_lightColor.size(); ++i )
        {
            if( !IsColorZero( m_boardAdapter.m_raytrace_lightColor[i] ) )
            {
                const SFVEC2F sc = m_boardAdapter.m_raytrace_lightSphericalCoords[i];

                m_lights.Add( new CDIRECTIONALLIGHT( SphericalToCartesian( glm::pi<float>() * sc.x,
                                                                           glm::pi<float>() * sc.y ),
                                                     m_boardAdapter.m_raytrace_lightColor[i] ) );
            }
        }
    }

    // Create an accelerator
    // /////////////////////////////////////////////////////////////////////////
    if( m_accelerator )
    {
        delete m_accelerator;
    }
    m_accelerator = 0;

    m_accelerator = new CBVH_PBRT( m_object_container, 8, SPLITMETHOD::MIDDLE );

    if( aStatusReporter )
    {
        // Calculation time in seconds
        const double calculation_time = (double)( GetRunningMicroSecs() -
                                                  stats_startReloadTime ) / 1e6;

        aStatusReporter->Report( wxString::Format( _( "Reload time %.3f s" ), calculation_time ) );
    }
}



// Based on draw3DViaHole from
// 3d_draw_helper_functions.cpp
void C3D_RENDER_RAYTRACING::insert3DViaHole( const VIA* aVia )
{
    PCB_LAYER_ID    top_layer, bottom_layer;
    int radiusBUI = (aVia->GetDrillValue() / 2);

    aVia->LayerPair( &top_layer, &bottom_layer );

    float topZ = m_boardAdapter.GetLayerBottomZpos3DU( top_layer ) +
                 m_boardAdapter.GetCopperThickness3DU();

    float botZ = m_boardAdapter.GetLayerBottomZpos3DU( bottom_layer ) -
                 m_boardAdapter.GetCopperThickness3DU();

    const SFVEC2F center = SFVEC2F( aVia->GetStart().x * m_boardAdapter.BiuTo3Dunits(),
                                    -aVia->GetStart().y * m_boardAdapter.BiuTo3Dunits() );

    CRING2D *ring = new CRING2D( center,
                                 radiusBUI * m_boardAdapter.BiuTo3Dunits(),
                                 ( radiusBUI + m_boardAdapter.GetHolePlatingThicknessBIU() ) *
                                 m_boardAdapter.BiuTo3Dunits(),
                                 *aVia );

    m_containerWithObjectsToDelete.Add( ring );


    CLAYERITEM *objPtr = new CLAYERITEM( ring, topZ, botZ );

    objPtr->SetMaterial( &m_materials.m_Copper );

    if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
        objPtr->SetColor( ConvertSRGBToLinear( (SFVEC3F)m_boardAdapter.m_CopperColor ) );
    else
        objPtr->SetColor( ConvertSRGBToLinear(
                m_boardAdapter.GetItemColor( LAYER_VIAS + static_cast<int>( aVia->GetViaType() ) ) ) );

    m_object_container.Add( objPtr );
}


// Based on draw3DPadHole from
// 3d_draw_helper_functions.cpp
void C3D_RENDER_RAYTRACING::insert3DPadHole( const PAD* aPad )
{
    const COBJECT2D *object2d_A = NULL;

    SFVEC3F objColor;

    if( m_boardAdapter.GetFlag( FL_USE_REALISTIC_MODE ) )
        objColor = (SFVEC3F)m_boardAdapter.m_CopperColor;
    else
        objColor = m_boardAdapter.GetItemColor( LAYER_PADS_TH );

    const wxSize  drillsize   = aPad->GetDrillSize();
    const bool    hasHole     = drillsize.x && drillsize.y;

    if( !hasHole )
        return;

    CONST_LIST_OBJECT2D antiOutlineIntersectionList;

    const float topZ = m_boardAdapter.GetLayerBottomZpos3DU( F_Cu ) +
                       m_boardAdapter.GetCopperThickness3DU() * 0.99f;

    const float botZ = m_boardAdapter.GetLayerBottomZpos3DU( B_Cu ) -
                       m_boardAdapter.GetCopperThickness3DU() * 0.99f;

    if( drillsize.x == drillsize.y )    // usual round hole
    {
        SFVEC2F center = SFVEC2F( aPad->GetPosition().x * m_boardAdapter.BiuTo3Dunits(),
                                  -aPad->GetPosition().y * m_boardAdapter.BiuTo3Dunits() );

        int innerRadius = drillsize.x / 2;
        int outerRadius = innerRadius + m_boardAdapter.GetHolePlatingThicknessBIU();

        CRING2D *ring = new CRING2D( center,
                                     innerRadius * m_boardAdapter.BiuTo3Dunits(),
                                     outerRadius * m_boardAdapter.BiuTo3Dunits(),
                                     *aPad );

        m_containerWithObjectsToDelete.Add( ring );

        object2d_A = ring;

        // If the object (ring) is intersected by an antioutline board,
        // it will use instease a CSG of two circles.
        if( object2d_A && !m_antioutlineBoard2dObjects->GetList().empty() )
        {

            m_antioutlineBoard2dObjects->GetListObjectsIntersects(
                        object2d_A->GetBBox(),
                        antiOutlineIntersectionList );
        }

        if( !antiOutlineIntersectionList.empty() )
        {
            CFILLEDCIRCLE2D *innerCircle = new CFILLEDCIRCLE2D( center,
                                                                innerRadius * m_boardAdapter.BiuTo3Dunits(),
                                                                *aPad );

            CFILLEDCIRCLE2D *outterCircle = new CFILLEDCIRCLE2D( center,
                                                                 outerRadius * m_boardAdapter.BiuTo3Dunits(),
                                                                 *aPad );
            std::vector<const COBJECT2D *> *object2d_B = new std::vector<const COBJECT2D *>();
            object2d_B->push_back( innerCircle );

            CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D( outterCircle,
                                                              object2d_B,
                                                              CSGITEM_FULL,
                                                              *aPad );

            m_containerWithObjectsToDelete.Add( itemCSG2d );
            m_containerWithObjectsToDelete.Add( innerCircle );
            m_containerWithObjectsToDelete.Add( outterCircle );

            object2d_A = itemCSG2d;
        }
    }
    else    // Oblong hole
    {
        wxPoint ends_offset;
        int     width;

        if( drillsize.x > drillsize.y )    // Horizontal oval
        {
            ends_offset.x = ( drillsize.x - drillsize.y ) / 2;
            width = drillsize.y;
        }
        else    // Vertical oval
        {
            ends_offset.y = ( drillsize.y - drillsize.x ) / 2;
            width = drillsize.x;
        }

        RotatePoint( &ends_offset, aPad->GetOrientation() );

        wxPoint start   = aPad->GetPosition() + ends_offset;
        wxPoint end     = aPad->GetPosition() - ends_offset;

        CROUNDSEGMENT2D *innerSeg = new CROUNDSEGMENT2D(
                                    SFVEC2F( start.x * m_boardAdapter.BiuTo3Dunits(),
                                             -start.y * m_boardAdapter.BiuTo3Dunits() ),
                                    SFVEC2F( end.x * m_boardAdapter.BiuTo3Dunits(),
                                             -end.y * m_boardAdapter.BiuTo3Dunits() ),
                                    width * m_boardAdapter.BiuTo3Dunits(),
                                    *aPad );

        CROUNDSEGMENT2D *outerSeg = new CROUNDSEGMENT2D(
                                    SFVEC2F( start.x * m_boardAdapter.BiuTo3Dunits(),
                                             -start.y * m_boardAdapter.BiuTo3Dunits() ),
                                    SFVEC2F( end.x * m_boardAdapter.BiuTo3Dunits(),
                                             -end.y * m_boardAdapter.BiuTo3Dunits() ),
                                    ( width + m_boardAdapter.GetHolePlatingThicknessBIU() * 2 ) * m_boardAdapter.BiuTo3Dunits(),
                                    *aPad );

        // NOTE: the round segment width is the "diameter", so we double the thickness

        std::vector<const COBJECT2D *> *object2d_B = new std::vector<const COBJECT2D *>();
        object2d_B->push_back( innerSeg );

        CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D( outerSeg,
                                                          object2d_B,
                                                          CSGITEM_FULL,
                                                          *aPad );

        m_containerWithObjectsToDelete.Add( itemCSG2d );
        m_containerWithObjectsToDelete.Add( innerSeg );
        m_containerWithObjectsToDelete.Add( outerSeg );

        object2d_A = itemCSG2d;

        if( object2d_A && !m_antioutlineBoard2dObjects->GetList().empty() )
        {

            m_antioutlineBoard2dObjects->GetListObjectsIntersects(
                        object2d_A->GetBBox(),
                        antiOutlineIntersectionList );
        }
    }


    if( object2d_A )
    {
        std::vector<const COBJECT2D *> *object2d_B = new std::vector<const COBJECT2D *>();

        // Check if there are any other THT that intersects this hole
        // It will use the non inflated holes
        if( !m_boardAdapter.GetThroughHole_Inner().GetList().empty() )
        {

            CONST_LIST_OBJECT2D intersectionList;
            m_boardAdapter.GetThroughHole_Inner().GetListObjectsIntersects( object2d_A->GetBBox(),
                                                                            intersectionList );

            if( !intersectionList.empty() )
            {
                for( CONST_LIST_OBJECT2D::const_iterator hole = intersectionList.begin();
                     hole != intersectionList.end();
                     ++hole )
                {
                    const COBJECT2D *hole2d = static_cast<const COBJECT2D *>(*hole);

                    if( object2d_A->Intersects( hole2d->GetBBox() ) )
                    //if( object2d_A->GetBBox().Intersects( hole2d->GetBBox() ) )
                        object2d_B->push_back( hole2d );
                }
            }
        }

        if( !antiOutlineIntersectionList.empty() )
        {
            for( const COBJECT2D *obj : antiOutlineIntersectionList )
            {
                object2d_B->push_back( obj );
            }
        }

        if( object2d_B->empty() )
        {
            delete object2d_B;
            object2d_B = CSGITEM_EMPTY;
        }

        if( object2d_B == CSGITEM_EMPTY )
        {
            CLAYERITEM *objPtr = new CLAYERITEM( object2d_A, topZ, botZ );

            objPtr->SetMaterial( &m_materials.m_Copper );
            objPtr->SetColor( ConvertSRGBToLinear( objColor ) );
            m_object_container.Add( objPtr );
        }
        else
        {
            CITEMLAYERCSG2D *itemCSG2d = new CITEMLAYERCSG2D( object2d_A,
                                                              object2d_B,
                                                              CSGITEM_FULL,
                                                              (const BOARD_ITEM &)*aPad );

            m_containerWithObjectsToDelete.Add( itemCSG2d );

            CLAYERITEM *objPtr = new CLAYERITEM( itemCSG2d, topZ, botZ );

            objPtr->SetMaterial( &m_materials.m_Copper );
            objPtr->SetColor( ConvertSRGBToLinear( objColor ) );

            m_object_container.Add( objPtr );
        }
    }
}


void C3D_RENDER_RAYTRACING::add_3D_vias_and_pads_to_container()
{
    // Insert plated vertical holes inside the board
    // /////////////////////////////////////////////////////////////////////////

    // Insert vias holes (vertical cylinders)
    for( TRACK* track : m_boardAdapter.GetBoard()->Tracks() )
    {
        if( track->Type() == PCB_VIA_T )
        {
            const VIA *via = static_cast<const VIA*>(track);
            insert3DViaHole( via );
        }
    }

    // Insert pads holes (vertical cylinders)
    for( FOOTPRINT* footprint : m_boardAdapter.GetBoard()->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
            if( pad->GetAttribute () != PAD_ATTRIB_NPTH )
            {
                insert3DPadHole( pad );
            }
    }
}


void C3D_RENDER_RAYTRACING::load_3D_models( CCONTAINER &aDstContainer, bool aSkipMaterialInformation )
{
    // Go for all footprints
    for( FOOTPRINT* fp : m_boardAdapter.GetBoard()->Footprints() )
    {
        if( !fp->Models().empty()
                && m_boardAdapter.ShouldFPBeDisplayed( (FOOTPRINT_ATTR_T) fp->GetAttributes() ) )
        {
            double zpos = m_boardAdapter.GetModulesZcoord3DIU( fp->IsFlipped() );

            wxPoint pos = fp->GetPosition();

            glm::mat4 fpMatrix = glm::mat4( 1.0f );

            fpMatrix = glm::translate( fpMatrix, SFVEC3F( pos.x * m_boardAdapter.BiuTo3Dunits(),
                                                          -pos.y * m_boardAdapter.BiuTo3Dunits(),
                                                          zpos ) );

            if( fp->GetOrientation() )
            {
                fpMatrix = glm::rotate( fpMatrix,
                                        ( (float)(fp->GetOrientation() / 10.0f) / 180.0f ) * glm::pi<float>(),
                                        SFVEC3F( 0.0f, 0.0f, 1.0f ) );
            }


            if( fp->IsFlipped() )
            {
                fpMatrix = glm::rotate( fpMatrix, glm::pi<float>(), SFVEC3F( 0.0f, 1.0f, 0.0f ) );

                fpMatrix = glm::rotate( fpMatrix, glm::pi<float>(), SFVEC3F( 0.0f, 0.0f, 1.0f ) );
            }

            const double modelunit_to_3d_units_factor = m_boardAdapter.BiuTo3Dunits() *
                                                        UNITS3D_TO_UNITSPCB;

            fpMatrix = glm::scale( fpMatrix,
                                   SFVEC3F( modelunit_to_3d_units_factor,
                                                modelunit_to_3d_units_factor,
                                                modelunit_to_3d_units_factor ) );

            BOARD_ITEM* boardItem = dynamic_cast<BOARD_ITEM*>( fp );

            // Get the list of model files for this model
            S3D_CACHE* cacheMgr = m_boardAdapter.Get3DCacheManager();
            auto sM = fp->Models().begin();
            auto eM = fp->Models().end();

            while( sM != eM )
            {
                if( ( static_cast<float>( sM->m_Opacity ) > FLT_EPSILON ) &&
                    ( sM->m_Show && !sM->m_Filename.empty() ) )
                {
                    // get it from cache
                    const S3DMODEL *modelPtr = cacheMgr->GetModel( sM->m_Filename );

                    // only add it if the return is not NULL
                    if( modelPtr )
                    {
                        glm::mat4 modelMatrix = fpMatrix;

                        modelMatrix = glm::translate( modelMatrix,
                                                      SFVEC3F( sM->m_Offset.x,
                                                               sM->m_Offset.y,
                                                               sM->m_Offset.z ) );

                        modelMatrix = glm::rotate( modelMatrix,
                                                   (float)-( sM->m_Rotation.z / 180.0f ) *
                                                   glm::pi<float>(),
                                                   SFVEC3F( 0.0f, 0.0f, 1.0f ) );

                        modelMatrix = glm::rotate( modelMatrix,
                                                   (float)-( sM->m_Rotation.y / 180.0f ) *
                                                   glm::pi<float>(),
                                                   SFVEC3F( 0.0f, 1.0f, 0.0f ) );

                        modelMatrix = glm::rotate( modelMatrix,
                                                   (float)-( sM->m_Rotation.x / 180.0f ) *
                                                   glm::pi<float>(),
                                                   SFVEC3F( 1.0f, 0.0f, 0.0f ) );

                        modelMatrix = glm::scale( modelMatrix,
                                                  SFVEC3F( sM->m_Scale.x,
                                                           sM->m_Scale.y,
                                                           sM->m_Scale.z ) );

                        add_3D_models( aDstContainer,
                                       modelPtr,
                                       modelMatrix,
                                       (float)sM->m_Opacity,
                                       aSkipMaterialInformation,
                                       boardItem );
                    }
                }

                ++sM;
            }
        }
    }
}

MODEL_MATERIALS *C3D_RENDER_RAYTRACING::get_3D_model_material( const S3DMODEL *a3DModel )
{
    MODEL_MATERIALS *materialVector;

    // Try find if the materials already exists in the map list
    if( m_model_materials.find( a3DModel ) != m_model_materials.end() )
    {
        // Found it, so get the pointer
        materialVector = &m_model_materials[a3DModel];
    }
    else
    {
        // Materials was not found in the map, so it will create a new for
        // this model.

        m_model_materials[a3DModel] = MODEL_MATERIALS();
        materialVector = &m_model_materials[a3DModel];

        materialVector->resize( a3DModel->m_MaterialsSize );

        for( unsigned int imat = 0;
             imat < a3DModel->m_MaterialsSize;
             ++imat )
        {
            if( m_boardAdapter.MaterialModeGet() == MATERIAL_MODE::NORMAL )
            {
                const SMATERIAL &material = a3DModel->m_Materials[imat];

                // http://www.fooplot.com/#W3sidHlwZSI6MCwiZXEiOiJtaW4oc3FydCh4LTAuMzUpKjAuNDAtMC4wNSwxLjApIiwiY29sb3IiOiIjMDAwMDAwIn0seyJ0eXBlIjoxMDAwLCJ3aW5kb3ciOlsiMC4wNzA3NzM2NzMyMzY1OTAxMiIsIjEuNTY5NTcxNjI5MjI1NDY5OCIsIi0wLjI3NDYzNTMyMTc1OTkyOTMiLCIwLjY0NzcwMTg4MTkyNTUzNjIiXSwic2l6ZSI6WzY0NCwzOTRdfV0-

                float reflectionFactor = 0.0f;

                if( (material.m_Shininess - 0.35f) > FLT_EPSILON )
                {
                    reflectionFactor = glm::clamp( glm::sqrt( (material.m_Shininess - 0.35f) ) *
                                                   0.40f - 0.05f,
                                                   0.0f,
                                                   0.5f );
                }

                CBLINN_PHONG_MATERIAL &blinnMaterial = (*materialVector)[imat];

                blinnMaterial = CBLINN_PHONG_MATERIAL(
                                          ConvertSRGBToLinear( material.m_Ambient ),
                                          ConvertSRGBToLinear( material.m_Emissive ),
                                          ConvertSRGBToLinear( material.m_Specular ),
                                          material.m_Shininess * 180.0f,
                                          material.m_Transparency,
                                          reflectionFactor );

                if( m_boardAdapter.GetFlag( FL_RENDER_RAYTRACING_PROCEDURAL_TEXTURES ) )
                {
                    // Guess material type and apply a normal perturbator

                    if( ( RGBtoGray( material.m_Diffuse ) < 0.3f ) &&
                        ( material.m_Shininess < 0.36f ) &&
                        ( material.m_Transparency == 0.0f ) &&
                        ( ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.g ) < 0.15f ) &&
                          ( glm::abs( material.m_Diffuse.b - material.m_Diffuse.g ) < 0.15f ) &&
                          ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.b ) < 0.15f ) ) )
                    {
                        // This may be a black plastic..
                        blinnMaterial.SetNormalPerturbator( &m_plastic_normal_perturbator );
                    }
                    else
                    {
                        if( ( RGBtoGray( material.m_Diffuse ) > 0.3f ) &&
                            ( material.m_Shininess < 0.30f ) &&
                            ( material.m_Transparency == 0.0f ) &&
                            ( ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.g ) > 0.25f ) ||
                              ( glm::abs( material.m_Diffuse.b - material.m_Diffuse.g ) > 0.25f ) ||
                              ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.b ) > 0.25f ) ) )
                        {
                            // This may be a color plastic ...
                            blinnMaterial.SetNormalPerturbator( &m_plastic_shine_normal_perturbator );
                        }
                        else
                        {
                            if( ( RGBtoGray( material.m_Diffuse ) > 0.6f ) &&
                                ( material.m_Shininess > 0.35f ) &&
                                ( material.m_Transparency == 0.0f ) &&
                                ( ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.g ) < 0.40f ) &&
                                  ( glm::abs( material.m_Diffuse.b - material.m_Diffuse.g ) < 0.40f ) &&
                                  ( glm::abs( material.m_Diffuse.r - material.m_Diffuse.b ) < 0.40f ) ) )
                            {
                                // This may be a brushed metal
                                blinnMaterial.SetNormalPerturbator( &m_brushed_metal_normal_perturbator );
                            }
                        }
                    }
                }
            }
            else
            {
                (*materialVector)[imat] = CBLINN_PHONG_MATERIAL( SFVEC3F( 0.2f ),
                                                                 SFVEC3F( 0.0f ),
                                                                 SFVEC3F( 0.0f ),
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f );
            }
        }
    }

    return materialVector;
}

void C3D_RENDER_RAYTRACING::add_3D_models( CCONTAINER &aDstContainer, const S3DMODEL *a3DModel,
                                           const glm::mat4 &aModelMatrix, float aFPOpacity,
                                           bool aSkipMaterialInformation, BOARD_ITEM *aBoardItem )
{

    // Validate a3DModel pointers
    wxASSERT( a3DModel != NULL );

    if( a3DModel == NULL )
        return;

    wxASSERT( a3DModel->m_Materials != NULL );
    wxASSERT( a3DModel->m_Meshes != NULL );
    wxASSERT( a3DModel->m_MaterialsSize > 0 );
    wxASSERT( a3DModel->m_MeshesSize > 0 );
    wxASSERT( aFPOpacity > 0.0f );
    wxASSERT( aFPOpacity <= 1.0f );

    if( aFPOpacity > 1.0f )
    {
        aFPOpacity = 1.0f;
    }

    if( (a3DModel->m_Materials != NULL) && (a3DModel->m_Meshes != NULL) &&
        (a3DModel->m_MaterialsSize > 0) && (a3DModel->m_MeshesSize > 0) )
    {

        MODEL_MATERIALS *materialVector = NULL;

        if( !aSkipMaterialInformation )
        {
            materialVector = get_3D_model_material( a3DModel );
        }

        const glm::mat3 normalMatrix = glm::transpose( glm::inverse( glm::mat3( aModelMatrix ) ) );

        for( unsigned int mesh_i = 0;
             mesh_i < a3DModel->m_MeshesSize;
             ++mesh_i )
        {
            const SMESH &mesh = a3DModel->m_Meshes[mesh_i];

            // Validate the mesh pointers
            wxASSERT( mesh.m_Positions != NULL );
            wxASSERT( mesh.m_FaceIdx != NULL );
            wxASSERT( mesh.m_Normals != NULL );
            wxASSERT( mesh.m_FaceIdxSize > 0 );
            wxASSERT( (mesh.m_FaceIdxSize % 3) == 0 );


            if( (mesh.m_Positions != NULL) &&
                (mesh.m_Normals != NULL) &&
                (mesh.m_FaceIdx != NULL) &&
                (mesh.m_FaceIdxSize > 0) &&
                (mesh.m_VertexSize > 0) &&
                ((mesh.m_FaceIdxSize % 3) == 0) &&
                (mesh.m_MaterialIdx < a3DModel->m_MaterialsSize) )
            {
                float fpTransparency;
                const CBLINN_PHONG_MATERIAL *blinn_material;

                if( !aSkipMaterialInformation )
                {
                    blinn_material = &(*materialVector)[mesh.m_MaterialIdx];

                    fpTransparency = 1.0f - (( 1.0f - blinn_material->GetTransparency() ) * aFPOpacity );
                }

                // Add all face triangles
                for( unsigned int faceIdx = 0;
                     faceIdx < mesh.m_FaceIdxSize;
                     faceIdx += 3 )
                {
                    const unsigned int idx0 = mesh.m_FaceIdx[faceIdx + 0];
                    const unsigned int idx1 = mesh.m_FaceIdx[faceIdx + 1];
                    const unsigned int idx2 = mesh.m_FaceIdx[faceIdx + 2];

                    wxASSERT( idx0 < mesh.m_VertexSize );
                    wxASSERT( idx1 < mesh.m_VertexSize );
                    wxASSERT( idx2 < mesh.m_VertexSize );

                    if( ( idx0 < mesh.m_VertexSize ) &&
                        ( idx1 < mesh.m_VertexSize ) &&
                        ( idx2 < mesh.m_VertexSize ) )
                    {
                        const SFVEC3F &v0 = mesh.m_Positions[idx0];
                        const SFVEC3F &v1 = mesh.m_Positions[idx1];
                        const SFVEC3F &v2 = mesh.m_Positions[idx2];

                        const SFVEC3F &n0 = mesh.m_Normals[idx0];
                        const SFVEC3F &n1 = mesh.m_Normals[idx1];
                        const SFVEC3F &n2 = mesh.m_Normals[idx2];

                        // Transform vertex with the model matrix
                        const SFVEC3F vt0 = SFVEC3F( aModelMatrix * glm::vec4( v0, 1.0f) );
                        const SFVEC3F vt1 = SFVEC3F( aModelMatrix * glm::vec4( v1, 1.0f) );
                        const SFVEC3F vt2 = SFVEC3F( aModelMatrix * glm::vec4( v2, 1.0f) );

                        const SFVEC3F nt0 = glm::normalize( SFVEC3F( normalMatrix * n0 ) );
                        const SFVEC3F nt1 = glm::normalize( SFVEC3F( normalMatrix * n1 ) );
                        const SFVEC3F nt2 = glm::normalize( SFVEC3F( normalMatrix * n2 ) );

                        CTRIANGLE *newTriangle = new  CTRIANGLE( vt0, vt2, vt1,
                                                                 nt0, nt2, nt1 );

                        newTriangle->SetBoardItem( aBoardItem );

                        aDstContainer.Add( newTriangle );

                        if( !aSkipMaterialInformation )
                        {
                            newTriangle->SetMaterial( blinn_material );
                            newTriangle->SetModelTransparency( fpTransparency );

                            if( mesh.m_Color == NULL )
                            {
                                const SFVEC3F diffuseColor =
                                    a3DModel->m_Materials[mesh.m_MaterialIdx].m_Diffuse;

                                if( m_boardAdapter.MaterialModeGet() == MATERIAL_MODE::CAD_MODE )
                                    newTriangle->SetColor( ConvertSRGBToLinear( MaterialDiffuseToColorCAD( diffuseColor ) ) );
                                else
                                    newTriangle->SetColor( ConvertSRGBToLinear( diffuseColor ) );
                            }
                            else
                            {
                                if( m_boardAdapter.MaterialModeGet() == MATERIAL_MODE::CAD_MODE )
                                    newTriangle->SetColor( ConvertSRGBToLinear( MaterialDiffuseToColorCAD( mesh.m_Color[idx0] ) ),
                                                           ConvertSRGBToLinear( MaterialDiffuseToColorCAD( mesh.m_Color[idx1] ) ),
                                                           ConvertSRGBToLinear( MaterialDiffuseToColorCAD( mesh.m_Color[idx2] ) ) );
                                else
                                    newTriangle->SetColor( ConvertSRGBToLinear( mesh.m_Color[idx0] ),
                                                           ConvertSRGBToLinear( mesh.m_Color[idx1] ),
                                                           ConvertSRGBToLinear( mesh.m_Color[idx2] ) );
                            }
                        }
                    }
                }
            }
        }
    }
}
