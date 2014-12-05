
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

#ifndef CO_MPICONNECTION_H
#define CO_MPICONNECTION_H

#include "connection.h"
#include "eventConnection.h"

#include <lunchbox/mpi.h>

namespace
{
typedef lunchbox::RefPtr< co::EventConnection > EventConnectionPtr;
}

namespace co
{

/* MPI connection
 * Allows peer-to-peer connections if the MPI runtime environment has
 * been set correctly.
 *
 * Due to Collage is a multithreaded library, MPI connections
 * requiere at least MPI_THREAD_SERIALIZED level of thread support.
 * During the initialization Collage will request the appropriate
 * thread support but if the MPI library does not provide it, MPI
 * connections will be disabled. If the application uses a MPI
 * connection when disabled, the connection should not be created.
 */
class MPIConnection : public Connection
{
    public:
        /** Construct a new MPI connection. */
        CO_API MPIConnection();

        /** Destruct this MPI connection. */
        CO_API ~MPIConnection();

        virtual bool connect();
        virtual bool listen();
        virtual void close() { _close(); }

        virtual void acceptNB();
        virtual ConnectionPtr acceptSync();

        virtual Notifier getNotifier() const;

        void setPeerRank( int32_t peerRank ) { _peerRank = peerRank; }
        void setTagSend( uint32_t tagSend ) { _tagSend = tagSend; }
        void setTagRecv( uint32_t tagRecv ) { _tagRecv = tagRecv; }
        int32_t getRank() { return _rank; }
        int32_t getPeerRank() { return _peerRank; }
        uint32_t getTagSend() { return _tagSend; }
        uint32_t getTagRecv() { return _tagRecv; }

    protected:
        void readNB( void* , const uint64_t ) { /* NOP */ }
        int64_t readSync( void* buffer, const uint64_t bytes,
                          const bool ignored);
        int64_t write( const void* buffer, const uint64_t bytes );

    private:
        void _close();
        uint64_t _copyData( void * buffer, const uint64_t bytes );

        int32_t     _rank;
        int32_t     _peerRank;
        uint32_t    _tagSend;
        uint32_t    _tagRecv;

        unsigned char * _buffer;
        unsigned char * _startBuffer;
        uint64_t         _bytesReceived;

        EventConnectionPtr  _event;
};

}
#endif //CO_MPICONNECTION_H
