
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

#include "mpiHandler.h"
#include "global.h"

#include <lunchbox/mtQueue.h>

#include <set>

#define END_PROBING         0xF
#define CONNECTION_REQUEST  0xE
#define CONNECTION_DONE     0xD
#define END_CONNECTION      0xC
#define END_ACCEPTING       0xB

lunchbox::MPI * co::MPIHandler::_mpi = 0;

namespace
{

/*
 * Every connection inside a process has a unique MPI tag.
 * This class allows to register a MPI tag and get a new unique tag.
 *
 * Due to the tag is defined by a 16 bits integer on
 * ConnectionDescription ( the real name is port but it
 * is reused for this purpuse ). The listeners always
 * use tags [ 1, 65535 ] and others [ 65536, MPI_TAG_UB ].
 */
static class TagManager
{
public:
    TagManager()
        : _nextTag( 65536 )
    {
    }

    bool registerTag(const uint32_t tag)
    {
        lunchbox::ScopedMutex< > mutex( _lock );

        LBASSERTINFO( tag != 0, "Tag 0 is reserved" );

        if( _tags.find( tag ) != _tags.end( ) )
            return false;

        _tags.insert( tag );

        return true;
    }

    void deregisterTag(const uint32_t tag)
    {
        lunchbox::ScopedMutex< > mutex( _lock );

        /** Ensure deregister tag is a register tag. */
        LBASSERT( _tags.find( tag ) != _tags.end( ) );

        _tags.erase( tag );
    }

    uint32_t getTag()
    {
        lunchbox::ScopedMutex< > mutex( _lock );

        do
        {
            _nextTag++;
            LBASSERT( _nextTag < MPI_TAG_UB - 1 );
        }
        while( _tags.find( _nextTag ) != _tags.end( ) );

        _tags.insert( _nextTag );

        return _nextTag;
    }

private:
    std::set< uint32_t >    _tags;
    uint32_t                _nextTag;
    lunchbox::Lock          _lock;
} tagManager;


struct NewConnection
{
    bool                error;
    int32_t             peerRank;
    uint32_t            tagRecv;
    uint32_t            tagSend;
    EventConnectionPtr  notifier;
    monitor_t           monitor;
};

struct Message
{
    Message( ) {}
    Message( MPI_Status  s, MPI_Message m )
            : status ( s )
            , msg( m )
    {
    }

    MPI_Status  status;
    MPI_Message msg;
};

typedef lunchbox::MTQueue< Message > messageQ_t;

struct ConnectionDetail
{
    messageQ_t          queue;
    int32_t             peerRank;
    uint32_t            tagClose;
    EventConnectionPtr  notifier;
    monitor_t           monitorRead;
    bool                connClosed;
};

typedef std::shared_ptr< NewConnection >    newConnection_ptr;
typedef std::shared_ptr< ConnectionDetail > connectionDetail_ptr;
typedef std::unordered_map< uint32_t, newConnection_ptr >
                                                        mapNewConnections_t;
typedef std::unordered_map< uint32_t, connectionDetail_ptr >
                                                        mapConnectionDetail_t;

mapNewConnections_t  mapListeners;
lunchbox::Lock       lockListeners;

mapNewConnections_t  mapConnects;
lunchbox::Lock       lockConnects;

mapConnectionDetail_t mapCommunications;
lunchbox::Lock        lockCommunications;

co::MPIHandler * mpiHandler = 0;

}

namespace co
{

lunchbox::MPI* MPIHandler::startMPI( int& argc, char**& argv )
{
    if( !_mpi )
    {
        static lunchbox::MPI mpi( argc, argv );
        static MPIHandler _mpiHandler;
        _mpi = &mpi;
        mpiHandler = &_mpiHandler;
    }

    return _mpi;
}

MPIHandler* MPIHandler::getInstance()
{
    LBASSERTINFO( mpiHandler, "MPIHandler::startMPI must be called first" );
    return mpiHandler;
}

void MPIHandler::_close()
{
    _running = true;
    _stopProbing();
    join();
}

void MPIHandler::run()
{
    while( _running.waitEQ( true ) )
    {
        MPI_Status status;
        MPI_Message msg;

        if( MPI_SUCCESS != MPI_Mprobe( MPI_ANY_SOURCE,
                                        MPI_ANY_TAG,
                                        MPI_COMM_WORLD,
                                        &msg,
                                        &status ) )
        {
            LBERROR << "Error retrieving messages " << std::endl;
            abort();
        }

        if( status.MPI_TAG == 0 ) // Protocol
        {
            if( _mpiProtocol( status, msg ) )
                break;
        }
        else
            _processMessage( status, msg );
    }
}

bool MPIHandler::registerTagListener( const uint32_t tag )
{
    return tagManager.registerTag( tag );
}

bool MPIHandler::acceptNB( const uint32_t tag, EventConnectionPtr notifier )
{
    lunchbox::ScopedMutex<> mutex( lockListeners );

    if( mapListeners.find( tag ) != mapListeners.end() )
    {
        LBERROR << "Tag " << tag << " already accepting" << std::endl;
        return false;
    }

    newConnection_ptr &listener = mapListeners[ tag ];
    listener = newConnection_ptr( new NewConnection );
    listener->notifier = notifier;
    listener->monitor = false;

    _running = true;

    return true;
}

bool MPIHandler::acceptSync( const uint32_t tag, int32_t& peerRank,
                                uint32_t& tagRecv, uint32_t& tagSend )
{
    newConnection_ptr listener;
    {
        lunchbox::ScopedMutex<> mutex( lockListeners );
        mapNewConnections_t::const_iterator it =  mapListeners.find( tag );

        if( it == mapListeners.end() )
        {
            LBERROR << "Tag " << tag << " is not accepting" << std::endl;
            return false;
        }
        listener = it->second;
    }

    listener->monitor.waitEQ( true );

    peerRank = listener->peerRank;
    tagRecv = listener->tagRecv;
    tagSend = listener->tagSend;
    const bool err = listener->error;

    if( Global::mpi->getRank() == peerRank )
    {
        lunchbox::ScopedMutex<> mutex( lockConnects );
        int size = mapConnects.size();
        mapNewConnections_t::const_iterator itC = mapConnects.find( tagSend );
        if( itC == mapConnects.end() )
        {
            LBERROR << "Tag " << tagSend << " is not connecting" << std::endl;
            return false;
        }
        itC->second->tagSend = tagRecv;
        itC->second->error = err;
        itC->second->monitor = true;
    }

    {
        lunchbox::ScopedMutex<> mutex( lockListeners );
        mapListeners.erase( tag );
    }

    if( err )
        return false;

    return _startCommunication( tagRecv, peerRank, tagSend, listener->notifier );
}

void MPIHandler::acceptStop( const uint32_t tag, const int32_t rank )

{
    mapNewConnections_t::const_iterator it;
    {
        lunchbox::ScopedMutex<> mutex( lockListeners );
        it =  mapListeners.find( tag );

        if( it == mapListeners.end() )
        {
            return;
        }
    }
    unsigned char s[5];
    s[0] = END_ACCEPTING;
    memcpy( &s[1], &tag, 4 );
    if( MPI_SUCCESS != MPI_Send( &s, 5,
                                    MPI_BYTE,
                                    rank,
                                    0,
                                    MPI_COMM_WORLD ) )
    {
        LBERROR << "Error sending end of accepting" << std::endl;
        abort();
    }
}

bool MPIHandler::connect( const uint32_t tag, const int32_t peerRank,
                            uint32_t& tagRecv, uint32_t& tagSend,
                            EventConnectionPtr notifier )
{
    tagRecv = tagManager.getTag( );
    mapNewConnections_t::const_iterator it;
    newConnection_ptr connectP;
    {
        lunchbox::ScopedMutex<> mutex( lockConnects );
        it =  mapConnects.find( tagRecv );

        if( it != mapConnects.end() )
        {
            LBERROR << "Tag " << tagRecv << " is already register" << std::endl;
            return false;
        }

        newConnection_ptr &listener = mapConnects[ tagRecv ];
        listener = newConnection_ptr( new NewConnection );
        listener->monitor = false;
        connectP = listener;
    }

    if( Global::mpi->getRank() != peerRank )
    {
        unsigned char msg[9];
        msg[0] = CONNECTION_REQUEST;
        memcpy( &msg[1], &tag, 4 );
        memcpy( &msg[5], &tagRecv, 4 );
        if( MPI_SUCCESS != MPI_Ssend( msg, 9,
                                        MPI_BYTE,
                                        peerRank,
                                        0,
                                        MPI_COMM_WORLD ) )
        {
            LBWARN << "Error sending MPI tag to peer in a MPI connection."
                   << std::endl;
            return false;
        }

        _running = true;
    }
    else
    {
        tagSend = tagManager.getTag( );
        {
            lunchbox::ScopedMutex<> mutex( lockListeners );
            mapNewConnections_t::const_iterator itC =  mapListeners.find( tag );

            if( itC == mapListeners.end() )
            {
                LBERROR << "Tag " << tag << " is not accepting" << std::endl;
                return false;
            }
            itC->second->peerRank = peerRank;
            itC->second->tagRecv = tagSend;
            itC->second->tagSend = tagRecv;
            itC->second->error = false;
            itC->second->notifier->set();
            itC->second->monitor = true;
        }
    }

    connectP->monitor.waitEQ( true );

    tagSend = connectP->tagSend;
    const bool err = connectP->error;

    {
        lunchbox::ScopedMutex<> mutex( lockConnects );
        mapConnects.erase( tag );
    }

    if( err )
        return false;

    return _startCommunication( tagRecv, peerRank, tagSend, notifier );
}

void MPIHandler::closeCommunication( const uint32_t tag )
{
    int32_t rank = -1;
    uint32_t tC = 0;
    {
        lunchbox::ScopedMutex<> mutex( lockCommunications );
        mapConnectionDetail_t::const_iterator it =
                                mapCommunications.find( tag );
        LBASSERT( it != mapCommunications.end() );
        rank = it->second->peerRank;
        tC = it->second->tagClose;
        if( it->second->connClosed )
        {
            mapCommunications.erase( it );
        }
        else
        {
            it->second->connClosed = true;
        }
    }

    unsigned char s[5];
    s[0] = END_CONNECTION;
    memcpy( &s[1], &tC, 4 );
    if( MPI_SUCCESS != MPI_Send( &s, 5,
                                    MPI_BYTE,
                                    rank,
                                    0,
                                    MPI_COMM_WORLD ) )
    {
        LBERROR << "Error sending end of connection" << std::endl;
        abort();
    }
}

bool MPIHandler::recvMsg( const uint32_t tag,
                          unsigned char*& buffer,
                          uint64_t& bytes )
{
    Message m;
    mapConnectionDetail_t::const_iterator it;
    connectionDetail_ptr conn;
    {
        lunchbox::ScopedMutex<> mutex( lockCommunications );
        it = mapCommunications.find( tag );
        if( it == mapCommunications.end() )
            return false;
        conn = it->second;
    }

    conn->monitorRead.waitEQ( true );
    if( conn->connClosed )
        return false;

    LBASSERT( !conn->queue.isEmpty() );
    if( !conn->queue.timedPop( co::Global::getTimeout(), m ) )
        return false;

    int b = 0;
    if( MPI_SUCCESS != MPI_Get_count( &m.status, MPI_BYTE, &b ) )
    {
        LBERROR << "Error retrieving messages " << std::endl;
        return false;
    }

    unsigned char * buff = new unsigned char[ b ];
    if( MPI_SUCCESS != MPI_Mrecv( buff, b, MPI_BYTE,
                                      &m.msg, MPI_STATUS_IGNORE ) )
    {
        LBERROR << "Error retrieving messages" << std::endl;
        delete buff;
        return false;
    }

    bytes = b;
    buffer = buff;

    return true;
}

bool MPIHandler::sendMsg( const int32_t rank, const uint32_t tag,
                           const void* buffer, const uint64_t bytes )
{
    if( MPI_SUCCESS != MPI_Send( &buffer, bytes,
                                    MPI_BYTE,
                                    rank,
                                    tag,
                                    MPI_COMM_WORLD ) )
    {
        return false;
    }

    return true;
}

bool MPIHandler::_startCommunication( const uint32_t tag,
                                     const int32_t rank,
                                     const uint32_t tagClose,
                                     EventConnectionPtr notifier )
{
    {
        lunchbox::ScopedMutex<> mutex( lockCommunications );

        if( mapCommunications.find( tag ) != mapCommunications.end() )
        {
            LBERROR << "Tag " << tag << " already accepting" << std::endl;
            return false;
        }
        connectionDetail_ptr &conn = mapCommunications[ tag ];
        conn = connectionDetail_ptr( new ConnectionDetail );
        conn->monitorRead = false;
        conn->connClosed = false;
        conn->tagClose = tagClose;
        conn->peerRank = rank;
        conn->notifier = notifier;
    }

    return true;
}


bool MPIHandler::_mpiProtocol( MPI_Status& status, MPI_Message& msg )
{
    int bytes = 0;
    unsigned char message[16];
    /** Consult number of bytes received. */
    if( MPI_SUCCESS != MPI_Get_count( &status, MPI_BYTE, &bytes ) )
    {
        LBERROR << "Error retrieving messages " << std::endl;
        abort();
    }

    LBASSERT( bytes <= 16 );

    if( MPI_SUCCESS != MPI_Mrecv( message, bytes, MPI_BYTE,
                                      &msg, MPI_STATUS_IGNORE ) )
    {
        LBERROR << "Error retrieving messages" << std::endl;
        abort();
    }

    switch( message[0] )
    {
        case END_PROBING:
            _running = false;
            return true;
        case CONNECTION_REQUEST:
            _acceptConnection( message, status );
            break;
        case CONNECTION_DONE:
            _connectionDone( message );
            break;
        case END_CONNECTION:
            _connectionClosed( message );
            break;
        case END_ACCEPTING:
            _acceptingClosed( message );
            break;
        default:
            LBWARN << "Package not identified" << std::endl;
    }
    return false;
}

void MPIHandler::_acceptConnection( unsigned char * message,
                                    MPI_Status& status )
{
    const uint32_t tag = *( (uint32_t*)&message[1] );
    LBASSERT( tag > 0 );

    mapNewConnections_t::const_iterator it;
    {
        lunchbox::ScopedMutex<> mutex( lockListeners );
        it =  mapListeners.find( tag );

        if( it == mapListeners.end() )
        {
            LBERROR << "Tag " << tag << " is not accepting" << std::endl;
            return;
        }
    }

    const uint32_t tS = *( (uint32_t*) &message[5] );
    LBASSERT( tS > 0 );
    const int32_t peerRank = status.MPI_SOURCE;
    const uint32_t tR = tagManager.getTag( );

    // Send Tag
    unsigned char msg[9];
    msg[0] = CONNECTION_DONE;
    memcpy( &msg[1], &tS, 4 );
    memcpy( &msg[5], &tR, 4 );
    if( MPI_SUCCESS != MPI_Ssend( msg, 9,
                                    MPI_BYTE,
                                    peerRank,
                                    0,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Error sending MPI tag to peer in a MPI connection."
               << std::endl;
        it->second->error = true;
    }
    else
    {
        it->second->peerRank = peerRank;
        it->second->tagRecv  = tR;
        it->second->tagSend  = tS;
        it->second->error    = false;
    }

    it->second->notifier->set();
    it->second->monitor = true;
}

void MPIHandler::_connectionDone( unsigned char * message )
{
    const uint32_t tag = *( (uint32_t*) &message[1] );
    LBASSERT( tag > 0 );

    mapNewConnections_t::const_iterator it;
    {
        lunchbox::ScopedMutex<> mutex( lockConnects );
        it =  mapConnects.find( tag );

        if( it == mapConnects.end() )
        {
            LBERROR << "Tag " << tag << " is not accepting" << std::endl;
            return;
        }
    }

    const uint32_t tS = *( (uint32_t*) &message[5] );
    LBASSERT( tS > 0 );

    it->second->tagSend  = tS;
    it->second->error    = false;
    it->second->monitor  = true;
}

void MPIHandler::_connectionClosed( unsigned char * message )
{
    const uint32_t tag = *( (uint32_t*) &message[1] );
    {
        lunchbox::ScopedMutex<> mutex( lockCommunications );
        mapConnectionDetail_t::const_iterator it =
                                mapCommunications.find( tag );
        LBASSERTINFO( it != mapCommunications.end(),
                                "Closing connection on tag " << tag );
        if( it->second->connClosed )
        {
            mapCommunications.erase( it );
        }
        else
        {
            it->second->connClosed = true;
            it->second->monitorRead = true;
            it->second->notifier->set();
        }
    }
}

void MPIHandler::_acceptingClosed( unsigned char * message )
{
    const uint32_t tag = *( (uint32_t*) &message[1] );
    {
        lunchbox::ScopedMutex<> mutex( lockListeners );
        mapNewConnections_t::const_iterator
                                it =  mapListeners.find( tag );

        LBASSERT( it == mapListeners.end() )
        it->second->error = true;
        it->second->monitor = true;
        it->second->notifier->set();
    }
}

void MPIHandler::_processMessage( MPI_Status& status, MPI_Message& msg )
{
    const uint32_t tag = status.MPI_TAG;
    {
        lunchbox::ScopedMutex<> mutex( lockCommunications );
        mapConnectionDetail_t::const_iterator it =
                                mapCommunications.find( tag );
        LBASSERTINFO( it != mapCommunications.end(),
                                "Tag " << tag << " is not register" );
        if( it->second->connClosed )
        {
            mapCommunications.erase( it );
        }
        else
        {
            it->second->queue.push( Message( status, msg ) );
            it->second->monitorRead = true;
            it->second->notifier->set();
        }
    }
}

void MPIHandler::_stopProbing()
{
    char s = END_PROBING;
    if( MPI_SUCCESS != MPI_Ssend( (void*)&s, 1,
                                    MPI_BYTE,
                                    Global::mpi->getRank(),
                                    0,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Write error, pausing mpi handler" << std::endl;
        abort();
    }
}

}
