
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

#include "mpiConnection.h"
#include "mpiHandler.h"
#include "connectionDescription.h"
#include "global.h"

#include <lunchbox/mtQueue.h>
#include <lunchbox/thread.h>
#include <lunchbox/condition.h>

#include <memory>
#include <map>
#include <set>

namespace
{
typedef lunchbox::RefPtr< co::EventConnection > EventConnectionPtr;
}

namespace co
{

MPIConnection::MPIConnection()
        : _rank( -1 )
        , _peerRank( -1 )
        , _tagSend( 0 )
        , _tagRecv( 0 )
        , _buffer( 0 )
        , _startBuffer( 0 )
        , _bytesReceived( 0 )
        , _event( new EventConnection )
{
    // Ask rank of the process
    _rank = Global::mpi->getRank();

    LBASSERT( _rank >= 0 );
    ConnectionDescriptionPtr description = _getDescription( );
    description->type = CONNECTIONTYPE_MPI;
    description->bandwidth = 1024000; // For example :S

    LBCHECK( _event->connect( ));
}

MPIConnection::~MPIConnection()
{
    _close();
}

co::Connection::Notifier MPIConnection::getNotifier() const
{
    if( isConnected() || isListening() )
        return _event->getNotifier();

    return -1;
}

bool MPIConnection::connect()
{
    LBASSERT( getDescription()->type == CONNECTIONTYPE_MPI );

    if( !isClosed() )
        return false;

    _setState( STATE_CONNECTING );

    ConnectionDescriptionPtr description = _getDescription( );
    _peerRank = description->rank;

    const int32_t cTag = description->port;

    if( !MPIHandler::getInstance()->connect( cTag, _peerRank,
                                                _tagRecv, _tagSend, _event ) )
    {
        LBWARN << "Could not connect to "
               << _peerRank << " process." << std::endl;
        _close();
        return false;
    }

    /** Check tag is correct. */
    LBASSERT( _tagSend > 0 );

    _setState( STATE_CONNECTED );

    LBINFO << "Connected with rank " << _peerRank << " on tag "
           << _tagRecv <<  std::endl;

    return true;
}

bool MPIConnection::listen()
{
    if( !isClosed())
        return false;

    if( isListening( ) )
    {
        LBINFO << "Probably, listen has already called before."
               << std::endl;
        _close();
        return false;
    }

    /** Set tag for listening. */
    _tagRecv = getDescription()->port;

    if( !MPIHandler::getInstance()->registerTagListener( _tagRecv ) )
    {
       LBERROR << "Tag already register" << std::endl;
       _close();
       return false;
    }

    LBINFO << "MPI Connection, rank " << _rank
           << " listening on tag " << _tagRecv << std::endl;

    _setState( STATE_LISTENING );

    return true;
}

void MPIConnection::_close()
{
    if( isClosed() )
        return;
    if( isListening() )
        MPIHandler::getInstance()->acceptStop( _tagRecv, _rank );

    if( isConnected() )
        MPIHandler::getInstance()->closeCommunication( _tagRecv );

    _setState( STATE_CLOSING );

    _event->close();

    _setState( STATE_CLOSED );
}

void MPIConnection::acceptNB()
{
    LBASSERT( isListening());

    /** Ensure tag is register. */
    LBASSERT( _tagRecv != 0 );

    if( !MPIHandler::getInstance()->acceptNB( _tagRecv, _event ) )
    {
        LBWARN << "Error accepting a MPI connection, closing connection."
               << std::endl;
        _close();
    }
}

ConnectionPtr MPIConnection::acceptSync()
{
    if( !isListening( ))
        return 0;

    int32_t  peerRank = -1;
    uint32_t tagS = 0;
    uint32_t tagR = 0;
    if( !MPIHandler::getInstance()->acceptSync( _tagRecv, peerRank,
                                                                tagR, tagS ) )
    {
        LBWARN << "Error accepting a MPI connection, closing connection."
               << std::endl;
        _close();
        return 0;
    }

    LBASSERT( peerRank >= 0 && tagS > 0 && tagR > 0 );

    MPIConnection * newConn = new MPIConnection( );
    newConn->setPeerRank( peerRank );
    newConn->setTagRecv( tagR );
    newConn->setTagSend( tagS );

    /** Start dispatcher of new connection. */
    newConn->_setState( STATE_CONNECTED );

    LBINFO << "Accepted to rank " << newConn->getPeerRank() << " on tag "
           << newConn->getTagRecv() << std::endl;

    _event->reset();

    return newConn;
}

uint64_t MPIConnection::_copyData( unsigned char * buffer,
                                   const uint64_t bytes )
{
    int64_t bytesRead = 0;

    if( _bytesReceived > 0 )
    {
        if( _bytesReceived >= bytes )
        {
            memcpy( buffer, _buffer, bytes );
            _bytesReceived -= bytes;
            bytesRead = bytes;
            _buffer += bytes;
        }
        else
        {
            memcpy( buffer, _buffer, _bytesReceived );
            bytesRead = _bytesReceived;
            _bytesReceived = 0;
        }

        if( _bytesReceived == 0)
        {
            _bytesReceived = 0;
            _buffer = 0;
            delete _startBuffer;
            _startBuffer = 0;
            _event->reset();
        }
    }

    return bytesRead;
}

int64_t MPIConnection::readSync( void* buffer,
                                 const uint64_t bytes,
                                 const bool )
{
    if( !isConnected() )
        return -1;

    unsigned char * buff = (unsigned char*) buffer;
    uint64_t bytesToRead = bytes;
    uint64_t bytesRead   = _copyData( buff, bytes );
    bytesToRead -= bytesRead;
    buff += bytesRead;

    while( bytesToRead > 0 )
    {
        LBASSERT( !_startBuffer  && _bytesReceived == 0 );

        if( !MPIHandler::getInstance()->recvMsg( _tagRecv,
                                            _startBuffer, _bytesReceived ) )
        {
            LBINFO << "Read error, closing connection" << std::endl;
            _close();
            return -1;
        }
        _buffer = _startBuffer;

        uint64_t b = _copyData( buff, bytesToRead );
        bytesToRead -= b;
        buff  += b;
    }

    return bytes;
}

int64_t MPIConnection::write( const void* buffer, const uint64_t bytes )
{
    if( !isConnected() )
        return -1;

    if( !MPIHandler::getInstance()->sendMsg( _peerRank, _tagSend,
                                                        buffer, bytes ) )
    {
        LBWARN << "Write error, closing connection" << std::endl;
        _close();
        return -1;
    }

    return bytes;
}

}
