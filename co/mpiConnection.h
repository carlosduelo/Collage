
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

#include <co/connection.h>

#include <lunchbox/thread.h>

#include <mpi.h>

#include <map>

namespace co
{
namespace detail { class MPIConnection; }

/* MPI connection
 * Allows peer-to-peer connections if the MPI runtime environment has
 * been set correctly.
 *
 * The maximum message size can be specified, if not, by defaul
 * it is 4KB. Changing this value allow to avoid segmentate the
 * message when sending, it the message is greater than 4KB.
 *
 * Due to Collage is a multithreaded library, MPI connections
 * requiere at least MPI_THREAD_SERIALIZED level of thread support.
 * During the initialization Collage will request the appropriate
 * thread support but if the MPI library does not provide it, MPI
 * connections will be disabled. If the application uses a MPI
 * connection when disabled, the library will abort.
 */
class MPIConnection : public Connection
{
    public:
        /** Construct a new MPI connection. */
        CO_API MPIConnection();

        MPIConnection(detail::MPIConnection * impl);

        /** Destruct this MPI connection. */
        CO_API ~MPIConnection();

        virtual bool connect();
        virtual bool listen();
        virtual void close();

        virtual void acceptNB();
        virtual ConnectionPtr acceptSync();

        virtual Notifier getNotifier() const { return _notifier; }

        /* Set the maximum message size. 
         * Only allow it in listener connections.
         *
         * @parama maximumm message size in bytes.
         */
        void setMaximumMessageSize(uint64_t size);

    protected:
        void readNB( void* , const uint64_t ) override { /* NOP */ }
        int64_t readSync( void* buffer, const uint64_t bytes, const bool ignored);
        int64_t write( const void* buffer, const uint64_t bytes );

    private:
        Notifier                            _notifier;
        detail::MPIConnection * const       _impl;

        void _close( const bool userClose );
};

}
#endif //CO_MPICONNECTION_H
