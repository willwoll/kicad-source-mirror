/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 KiCad Developers, see change_log.txt for contributors.
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

#include <fctsys.h>
#include <netlist_reader/pcb_netlist.h>
#include <drc/footprint_tester.h>
#include <drc/drc_item.h>

void TestFootprints( NETLIST& aNetlist, BOARD* aBoard, std::vector<std::shared_ptr<DRC_ITEM> >& aDRCList )
{
    wxString msg;

    auto comp = []( const MODULE* x, const MODULE* y )
    {
        return x->GetReference().CmpNoCase( y->GetReference() ) < 0;
    };
    auto mods = std::set<MODULE*, decltype( comp )>( comp );

    if( !aBoard->GetDesignSettings().Ignore( DRCE_DUPLICATE_FOOTPRINT ) )
    {
        // Search for duplicate footprints on the board
        for( MODULE* mod : aBoard->Modules() )
        {
            auto ins = mods.insert( mod );

            if( !ins.second )
            {
                std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_DUPLICATE_FOOTPRINT );
                item->SetItems( mod, *ins.first );
                aDRCList.push_back( item );
            }
        }
    }

    // Search for component footprints in the netlist but not on the board.
    for( unsigned ii = 0; ii < aNetlist.GetCount(); ii++ )
    {
        COMPONENT* component = aNetlist.GetComponent( ii );
        MODULE*    module = aBoard->FindModuleByReference( component->GetReference() );

        if( module == nullptr )
        {
            if( !aBoard->GetDesignSettings().Ignore( DRCE_MISSING_FOOTPRINT ) && module == NULL )
            {
                msg.Printf( _( "Missing footprint %s (%s)" ),
                            component->GetReference(),
                            component->GetValue() );

                std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_MISSING_FOOTPRINT );
                item->SetErrorMessage( msg );
                aDRCList.push_back( item );
            }
        }
        else
        {
            if( !aBoard->GetDesignSettings().Ignore( DRCE_NET_CONFLICT ) )
            {
                for( D_PAD* pad : module->Pads() )
                {
                    const COMPONENT_NET& sch_net = component->GetNet( pad->GetName() );
                    const wxString&      pcb_netname = pad->GetNetname();

                    if( !pcb_netname.IsEmpty() && sch_net.GetPinName().IsEmpty() )
                    {
                        msg.Printf( _( "No corresponding pin found in schematic." ) );

                        std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_NET_CONFLICT );
                        item->SetErrorMessage( msg );
                        item->SetItems( pad );
                        aDRCList.push_back( item );
                    }
                    else if( pcb_netname.IsEmpty() && !sch_net.GetNetName().IsEmpty() )
                    {
                        msg.Printf( _( "Pad missing net given by schematic (%s)." ),
                                    sch_net.GetNetName() );

                        std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_NET_CONFLICT );
                        item->SetErrorMessage( msg );
                        item->SetItems( pad );
                        aDRCList.push_back( item );
                    }
                    else if( pcb_netname != sch_net.GetNetName() )
                    {
                        msg.Printf( _( "Pad net (%s) doesn't match net given by schematic (%s)." ),
                                    pcb_netname,
                                    sch_net.GetNetName() );

                        std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_NET_CONFLICT );
                        item->SetErrorMessage( msg );
                        item->SetItems( pad );
                        aDRCList.push_back( item );
                    }
                }

                for( unsigned jj = 0; jj < component->GetNetCount(); ++jj )
                {
                    const COMPONENT_NET& sch_net = component->GetNet( jj );

                    if( !module->FindPadByName( sch_net.GetPinName() ) )
                    {
                        msg.Printf( _( "No pad found for pin %s in schematic." ),
                                    sch_net.GetPinName() );

                        std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_NET_CONFLICT );
                        item->SetErrorMessage( msg );
                        item->SetItems( module );
                        aDRCList.push_back( item );
                    }
                }
            }
        }
    }

    if( !aBoard->GetDesignSettings().Ignore( DRCE_EXTRA_FOOTPRINT ) )
    {
        // Search for component footprints found on board but not in netlist.
        for( MODULE* module : aBoard->Modules() )
        {
            COMPONENT* component = aNetlist.GetComponentByReference( module->GetReference() );

            if( component == NULL )
            {
                std::shared_ptr<DRC_ITEM> item = DRC_ITEM::Create( DRCE_EXTRA_FOOTPRINT );
                item->SetItems( module );
                aDRCList.push_back( item );
            }
        }
    }
}