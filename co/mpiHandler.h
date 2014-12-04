
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

#ifndef CO_MPIHANDLER_H
#define CO_MPIHANDLER_H

#include "global.h"
#include "eventConnection.h"

#include <lunchbox/thread.h>
#include <lunchbox/scopedMutex.h>
#include <lunchbox/monitor.h>
#include <lunchbox/mpi.h>

#include <memory>
#include <unordered_map>

#include <mpi.h>

namespace
{
typedef lunchbox::RefPtr< co::EventConnection > EventConnectionPtr;
typedef lunchbox::Monitor< bool > monitor_t;
}

namespace co
{

class MPIHandler : lunchbox::Thread
{
public:
    static lunchbox::MPI* startMPI( int& argc, char**& argv );

    static MPIHandler* getInstance();

    virtual void run();

    bool registerTagListener( const uint32_t tag );

    bool acceptNB( const uint32_t tag, EventConnectionPtr notifier );

    bool acceptSync( const uint32_t tag, int32_t& peerRank,
                        uint32_t& tagRecv, uint32_t& tagSend );

    void acceptStop( const uint32_t tag, const int32_t rank );

    bool connect( const uint32_t tag, const int32_t peerRank,
                        uint32_t& tagRecv, uint32_t& tagSend,
                        EventConnectionPtr notifier  );

    void closeCommunication( const uint32_t tag );

    bool recvMsg( const uint32_t tag, unsigned char*& buffer, uint64_t& bytes );

    bool sendMsg( const int32_t rank, const uint32_t tag,
                        const void* buffer, const uint64_t bytes );
private:
    MPIHandler() : _running( false ) { start(); }

    MPIHandler( MPIHandler const& );

    ~MPIHandler(){ _close(); }

    void operator=( MPIHandler const& );

    monitor_t _running;

    bool _startCommunication( const uint32_t tag, const int32_t rank,
                                const uint32_t tagClose,
                                EventConnectionPtr notifier );
    bool _mpiProtocol( MPI_Status& status, MPI_Message& msg );
    void _acceptConnection( unsigned char * message, MPI_Status& status );
    void _connectionDone( unsigned char * message );
    void _connectionClosed( unsigned char * message );
    void _acceptingClosed( unsigned char * message );
    void _processMessage( MPI_Status& status, MPI_Message& msg );
    void _stopProbing();

    void _close();

    static lunchbox::MPI * _mpi;
};

}

#endif // CO_MPIHANDLER_H
