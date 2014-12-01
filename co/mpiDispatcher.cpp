
/* Copyright (c) 2014, Carlos Duelo <cduelo@cesvima.upm.es>
 *
 * This file is part of Collage <https://github.com/Eyescale/Collage>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "co/mpiDispatcher.h"

#include "global.h"

#include <mpi.h>

namespace co
{

void MPIDispatcher::run()
{
    while( _running )
    {
        std::cout << "CLOOOOSE 1" << std::endl;
        while( _monitor.waitEQ( true ) && _running )
        {
            MPI_Status status;
            if( MPI_SUCCESS != MPI_Probe( MPI_ANY_SOURCE,
                                            MPI_ANY_TAG,
                                            MPI_COMM_WORLD,
                                            &status ) )
            {
                LBERROR << "Error retrieving messages " << std::endl;
                break;
            }

            if( status.MPI_TAG == 0 )
            {
                char s;
                if( MPI_SUCCESS != MPI_Recv( &s, 1, MPI_BYTE,
                                                Global::mpi->getRank(),
                                                0,
                                                MPI_COMM_WORLD,
                                                MPI_STATUS_IGNORE ) )
                {
                    LBERROR << "Error retrieving messages" << std::endl;
                }
            }
            else
            {
                lunchbox::ScopedMutex< > mutex( _lock );
                cond_ptr &entry = _clients[ status.MPI_TAG ]; 

                if( entry )
                    entry->signal();
                else
                    LBWARN << "Tag not resgister in MPIDispatcher" << std::endl;
            }
        }
    }

    LBASSERT( _clients.size() == 0 );
}

void MPIDispatcher::close()
{
    std::cout << "CLOSE MPI DISPATCHER"<<std::endl;
    LBASSERT( !_monitor );
    _running = false;
    _monitor = true;
    join();
}

void MPIDispatcher::registerClient( uint32_t tag )
{
    lunchbox::ScopedMutex< > mutex( _lock );
    cond_ptr &entry = _clients[ tag ]; 

    LBASSERTINFO( !entry, "Tag not resgister in MPIDispatcher" );

    entry = cond_ptr( new lunchbox::Condition() );
    _monitor = true;
}

void MPIDispatcher::deregisterClient( uint32_t tag )
{
    {
        lunchbox::ScopedMutex< > mutex( _lock );
        _clients.erase( tag );
    }

    if( _clients.size() == 0 )
    {
        _monitor = false;
        char s;
        if( MPI_SUCCESS != MPI_Send( (void*)&s, 1,
                                        MPI_BYTE,
                                        Global::mpi->getRank(),
                                        0,
                                        MPI_COMM_WORLD ) )
        {
            LBWARN << "Write error, closing connection" << std::endl;
        }
    }
}

bool MPIDispatcher::wait( uint32_t tag )
{
    _lock.set();
    cond_ptr &entry = _clients[ tag ]; 
    _lock.unset();

    LBASSERTINFO( entry, "Tag not resgister in MPIDispatcher" );

    return entry->timedWait( (const unsigned) co::Global::getTimeout() );
}
}
