
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

#include <set>

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


}

namespace co
{

void MPIHandler::close()
{
    LBASSERT( !_monitor );
    LBASSERT( _clients.size() == 0 && _listeners.size() == 0 );
    _running = false;
    _monitor = true;
    join();
}


void MPIHandler::run()
{
    while( _running )
    {
        while( _monitor.waitEQ( true ) && _running )
        {
            MPI_Status status;
            MPI_Message msg;

            _lockProbing.set();
            if( MPI_SUCCESS != MPI_Mprobe( MPI_ANY_SOURCE,
                                            MPI_ANY_TAG,
                                            MPI_COMM_WORLD,
                                            &msg,
                                            &status ) )
            {
                LBERROR << "Error retrieving messages " << std::endl;
                abort();
            }
            _lockProbing.unset();

            if( !_checkStopProbing( status, msg ) )
            {
                if ( !_checkListener( status, msg ) &&
                     !_checkMessage( status, msg ) )
                {
                    LBWARN << "Tag " << status.MPI_TAG
                           << " not resgister in MPIHandler" << std::endl;
                }
            }
        }

    }

    LBASSERT( _clients.size() == 0  && _listeners.size() == 0 );
}

bool MPIHandler::registerListener( const uint32_t tag,
                                    EventConnectionPtr notifier )
{
    /** Register tag. */
    if( !tagManager.registerTag( tag ) )
    {
        LBWARN << "Tag " << tag << " is already register." << std::endl;
        return false;
    }

    {
        lunchbox::ScopedMutex< > mutex( _lockData );
        listenerM::const_iterator it = _listeners.find( tag );
        LBASSERTINFO( it == _listeners.end(),
                                "Tag already resgister in MPIHandler" );

        listenerQ_ptr entry = _listeners[ tag ];


        entry = listenerQ_ptr( new lunchbox::MTQueue< Listener >() );
        _notifiers[ tag ] = notifier;
    }

    _monitor = true;
    std::cout << _listeners.size() <<" +++++++++++++++++ " << tag << std::endl;

    return true;
}

void MPIHandler::deregisterListener( const uint32_t tag )
{
    _lockData.set();

    listenerM::const_iterator it = _listeners.find( tag );

    if(  it == _listeners.end() )
    {
        _lockData.unset();
        return;
    }
    listenerQ_ptr entry = it->second;
    _listeners.erase( tag );
    _notifiers.erase( tag );
    std::cout << _listeners.size()<<" ================= " << tag << std::endl;

    _stopProbing();

    _lockData.unset();

    entry->push( Listener( 0, 0, 0, false ) );

    tagManager.deregisterTag( tag );
}

bool MPIHandler::waitListen( const uint32_t tag, int32_t& rank,
                               uint32_t& tagSend, uint32_t& tagRec )
{
    _lockData.set();

    listenerM::const_iterator it = _listeners.find( tag );

    LBASSERT(  it != _listeners.end() );

    listenerQ_ptr entry = it->second;

    _lockData.unset();

    Listener listener;
    if( !entry->timedPop( (const unsigned) co::Global::getTimeout(), listener ) )
        return false;

    if( !listener.valid )
        return false;

    rank = listener.rank;
    tagSend = listener.tagSend;
    tagRec = listener.tagRec;

    return true;
}

bool MPIHandler::registerClient( uint32_t tag, EventConnectionPtr notifier )
{
    {
        lunchbox::ScopedMutex< > mutex( _lockData );
        clientM::const_iterator it = _clients.find( tag );

        LBASSERTINFO( it == _clients.end(), "Tag already resgister in MPIHandler" );
        clientQ_ptr entry = it->second;

        entry = clientQ_ptr( new lunchbox::MTQueue< Message >() );
        _notifiers[ tag ] = notifier;
    }

    _monitor = true;

    return true;
}

void MPIHandler::deregisterClient( const uint32_t tag, const uint32_t rank,
                                        const uint32_t tagClose )
{
    _lockData.set();

    clientM::const_iterator it = _clients.find( tag );
    clientQ_ptr entry = it->second;

    if( it == _clients.end() )
    {
        _lockData.unset();
        return;
    }
    _clients.erase( tag );
    _notifiers.erase( tag );

    _stopProbing();

    _lockData.unset();

    _closeRemote( rank, tagClose );

    entry->push( Message( ) );

    tagManager.deregisterTag( tag );
}

bool MPIHandler::recvMsg( const uint32_t tag, unsigned char* buffer,
                             uint64_t& bytes, int32_t& rank )
{
    _lockData.set();
    clientQ_ptr entry = _clients[ tag ];
    _lockData.unset();

    LBASSERTINFO( entry, "Tag not resgister in MPIHandler" );

    Message msg;
    if( !entry->timedPop( (const unsigned) co::Global::getTimeout(), msg ) )
        return false;

    if( !msg.valid )
        return false;

    rank = msg.status.MPI_SOURCE;

    int bytesR = 0;
    /** Consult number of bytes received. */
    if( MPI_SUCCESS != MPI_Get_count( &msg.status, MPI_BYTE, &bytesR ) )
    {
        LBERROR << "Error retrieving messages " << std::endl;
        return false;
    }
    bytes = bytesR;

    buffer = new unsigned char[ bytes ];
    /* Receive the message, this call is not blocking due to the
     * previous MPI_Probe call.
     */
    if( MPI_SUCCESS != MPI_Mrecv( buffer, bytes, MPI_BYTE,
                                 &msg.msg,
                                 MPI_STATUS_IGNORE ) )
    {
        LBERROR << "Error retrieving messages " << std::endl;
        delete buffer;
        return false;
    }

    return true;
}

bool MPIHandler::sendMsg( const uint32_t rank, const uint32_t tag,
                             const void * buffer, const uint64_t bytes)
{
    if( MPI_SUCCESS != MPI_Send( (void*)buffer, bytes,
                                    MPI_BYTE,
                                    rank,
                                    tag,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Write error, closing connection" << std::endl;
        return false;
    }

    return true;
}

bool MPIHandler::connect( const int32_t rankS,
                          const int32_t rank,
                          const uint32_t tag,
                          uint32_t& tagSend,
                          uint32_t& tagRec )
{
    /** To connect first send the rank. */
    if( MPI_SUCCESS != MPI_Ssend( &rankS, 1,
                                    MPI_INT,
                                    rank,
                                    tag,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Could not connect to "
               << rank << " process." << std::endl;
        return false;
    }

    /* If the listener receive the rank, he should send
     * the MPI tag used for send.
     */
    if( MPI_SUCCESS != MPI_Recv( &tagSend, 1,
                                    MPI_INT,
                                    rank,
                                    tag,
                                    MPI_COMM_WORLD,
                                    MPI_STATUS_IGNORE ) )
    {
        LBWARN << "Could not receive MPI tag from "
               << rank << " process." << std::endl;
        return false;
    }

    /** Check tag is correct. */
    LBASSERT( tagSend > 0 );

    /** Get a new tag to receive and send it. */
    tagRec = ( int32_t )tagManager.getTag( );

    if( MPI_SUCCESS != MPI_Ssend( &tagRec, 1,
                                    MPI_INT,
                                    rank,
                                    tag,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Could not connect to "
               << rank << " process." << std::endl;
        return false;
    }

    return true;
}

bool MPIHandler::_closeRemote( const uint32_t rank, const uint32_t tag )
{

    if( MPI_SUCCESS != MPI_Ssend( &tag, 1,
                                    MPI_INT,
                                    rank,
                                    0,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Could not close remote " << rank << " in tag "
               << tag << std::endl;
        return false;
    }

    return true;
}

bool MPIHandler::_checkListener( MPI_Status& status, MPI_Message& msg )
{
    _lockData.set();
    listenerM::const_iterator it = _listeners.find( status.MPI_TAG );
    _lockData.unset();

    if(  it == _listeners.end() )
        return false;

    listenerQ_ptr entry = it->second;

    Listener listener;

    if( MPI_SUCCESS != MPI_Mrecv( &listener.rank, 1,
                                    MPI_INT,
                                    &msg,
                                    MPI_STATUS_IGNORE ) )
    {
        LBWARN << "Could not start accepting a MPI connection, "
               << "closing connection." << std::endl;
        return false;
    }

    LBASSERT( listener.rank == status.MPI_SOURCE );

    if( listener.rank < 0 )
    {
        LBINFO << "Error accepting connection from rank "
               << listener.rank << std::endl;
        return false;
    }

    listener.tagRec = ( int32_t )tagManager.getTag( );

    // Send Tag
    if( MPI_SUCCESS != MPI_Ssend( &listener.tagRec, 4,
                                    MPI_BYTE,
                                    listener.rank,
                                    status.MPI_TAG,
                                    MPI_COMM_WORLD ) )
    {
        LBWARN << "Error sending MPI tag to peer in a MPI connection."
               << std::endl;
        return false;
    }

    /** Receive the peer tag. */
    if( MPI_SUCCESS != MPI_Recv( &listener.tagSend, 4,
                                    MPI_BYTE,
                                    listener.rank,
                                    status.MPI_TAG,
                                    MPI_COMM_WORLD,
                                    MPI_STATUS_IGNORE ) )
    {
        LBWARN << "Could not receive MPI tag from "
               << listener.rank << " process." << std::endl;
        return false;
    }

    listener.valid = true;

    entry->push( listener );
    _notifiers[ status.MPI_TAG ]->set();

    std::cout << "NEw LISTEN " << std::endl;

    return true;
}

bool MPIHandler::_checkMessage( MPI_Status& status, MPI_Message& msg )
{
    _lockData.set();
    clientQ_ptr &entry = _clients[ status.MPI_TAG ];
    _lockData.unset();

    if( !entry )
        return false;

    entry->push( Message( status, msg, true ) );
    _notifiers[ status.MPI_TAG ]->set();

    return true;
}

bool MPIHandler::_checkStopProbing( MPI_Status& status, MPI_Message& msg )
{
    if( status.MPI_TAG == 0 )
    {
        int bytes = 0;
        /** Consult number of bytes received. */
        if( MPI_SUCCESS != MPI_Get_count( &status, MPI_BYTE, &bytes ) )
        {
            LBERROR << "Error retrieving messages " << std::endl;
            return false;
        }

        if( bytes == 1 ) // Stop probing
        {
            char s;
            if( MPI_SUCCESS != MPI_Mrecv( &s, 1, MPI_BYTE,
                                            &msg, MPI_STATUS_IGNORE ) )
            {
                LBERROR << "Error retrieving messages" << std::endl;
            }
            return true;
        }
        else
        {
            uint32_t tag;
            if( MPI_SUCCESS != MPI_Mrecv( &tag, 1, MPI_INT,
                                            &msg, MPI_STATUS_IGNORE ) )
            {
                LBERROR << "Error retrieving messages" << std::endl;
            }
            _lockData.set();

            clientQ_ptr entry = _clients[ tag ];
            LBASSERT( entry );
            _clients.erase( tag );
            _notifiers.erase( tag );

            _stopProbing();

            _lockData.unset();

            entry->push( Message( ) );

            tagManager.deregisterTag( tag );
        }
    }
    return false;
}

void MPIHandler::_stopProbing()
{
    const bool needStop = _clients.size() == 0 && _listeners.size() == 0;

    std::cout << _clients.size() << " " << _listeners.size() <<std::endl;

    if( !needStop )
        return;

    _monitor = false;

    if( !_lockProbing.trySet() )
    {
    std::cout <<"SEND IT"<<std::endl;
        char s;
        if( MPI_SUCCESS != MPI_Ssend( (void*)&s, 1,
                                        MPI_BYTE,
                                        Global::mpi->getRank(),
                                        0,
                                        MPI_COMM_WORLD ) )
        {
            LBWARN << "Write error, pausing mpi handler" << std::endl;
        }
        _lockProbing.set();
    }

    LBASSERT( !_monitor );

    _lockProbing.unset();
}

}
