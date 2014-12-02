
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

#ifndef CO_MPIDISPATCHER_H
#define CO_MPIDISPATCHER_H

#include "global.h"
#include "eventConnection.h"

#include <lunchbox/thread.h>
#include <lunchbox/scopedMutex.h>
#include <lunchbox/monitor.h>
#include <lunchbox/mtQueue.h>

#include <memory>
#include <unordered_map>

#include <mpi.h>

namespace
{
class MPIHandlerCloser;

typedef lunchbox::RefPtr< co::EventConnection > EventConnectionPtr;
}

namespace co
{

class MPIHandler : lunchbox::Thread
{
public:
    MPIHandler() : _running( true ), _monitor( false ) { start(); }

    ~MPIHandler() {}

    void close();

    virtual void run();

    bool registerListener( const uint32_t tag, EventConnectionPtr notifier );

    void deregisterListener( const uint32_t tag );

    bool waitListen( const uint32_t tag, int32_t& rank,
                   uint32_t& tagSend, uint32_t& tagRec );

    bool registerClient( const uint32_t tag, EventConnectionPtr notifier );

    void deregisterClient( const uint32_t tag );

    bool connect( const int32_t rankS, const int32_t rank, const uint32_t tag,
                   uint32_t& tagSend, uint32_t& tagRec );

    bool recvMsg( const uint32_t tag, unsigned char* buffer,
                    uint64_t& bytes, int32_t& rank );

    bool sendMsg( const uint32_t rank, const uint32_t tag,
                    const void * buffer, const uint64_t bytes );

private:
    class Message
    {
    public:
        Message( )
            : valid( false )
        {}
        Message( const MPI_Status s, const MPI_Message m, const bool v )
            : status( s )
            , msg( m )
            , valid( v )
        {}
        MPI_Status  status;
        MPI_Message msg;
        bool  valid;
    };

    class Listener
    {
    public:
        Listener( ) 
            : rank( -1 )
            , tagSend( 0 )
            , tagRec( 0 )
            , valid( false )
        {
        }
        Listener( const int32_t& r, const uint32_t& tS,
                        const uint32_t& tR, const bool v )
            : rank( r )
            , tagSend( tS )
            , tagRec( tR )
            , valid( v )
        {
        }
        int32_t rank;
        uint32_t tagSend;
        uint32_t tagRec;
        bool valid;
    };

    typedef std::shared_ptr< lunchbox::MTQueue< Message > >  clientQ_ptr;
    typedef std::shared_ptr< lunchbox::MTQueue< Listener > > listenerQ_ptr;
    typedef lunchbox::Monitor< bool >  monitor_t;

    monitor_t _running;
    monitor_t _monitor;
    lunchbox::Lock _lockProbing;

    lunchbox::Lock _lockData;
    std::unordered_map< uint32_t, clientQ_ptr >         _clients;
    std::unordered_map< uint32_t, listenerQ_ptr >       _listeners;
    std::unordered_map< uint32_t, EventConnectionPtr >  _notifiers;

    bool _checkListener( MPI_Status& status, MPI_Message& msg );
    bool _checkMessage( MPI_Status& status, MPI_Message& msg );
    bool _checkStopProbing( MPI_Status& status, MPI_Message& msg );
    void _stopProbing();
};

typedef std::unique_ptr< MPIHandler, MPIHandlerCloser > MPIHandler_ptr;

}

namespace
{
class MPIHandlerCloser
{
public:
    void operator()( co::MPIHandler * p )
    {
        if( p )
            p->close();
        delete p;
    }
};
}

#endif // CO_MPIDISPATCHER_H
