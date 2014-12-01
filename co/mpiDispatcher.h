
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

#include <lunchbox/thread.h>
#include <lunchbox/scopedMutex.h>
#include <lunchbox/monitor.h>
#include <lunchbox/mtQueue.h>

#include <memory>
#include <unordered_map>

#include <mpi.h>

namespace
{
class MPIDispatcherCloser;
}

namespace co
{

class MPIDispatcher : lunchbox::Thread
{
public:
    MPIDispatcher() : _running( true ), _monitor( false ) { start(); }

    ~MPIDispatcher() {}

    void close();

    virtual void run();

    void registerClient( uint32_t tag );

    void deregisterClient( uint32_t tag );

    bool wait( uint32_t tag, MPI_Status& status );


private:
    typedef std::unique_ptr< lunchbox::MTQueue< MPI_Status > > client_ptr;

    lunchbox::Monitor< bool >       _running;
    lunchbox::Monitor< bool >       _monitor;
    std::unordered_map< uint32_t, client_ptr >  _clients;
    lunchbox::Lock                  _lock;
};

typedef std::unique_ptr< MPIDispatcher, MPIDispatcherCloser > MPIDispatcher_ptr;

}

namespace
{
class MPIDispatcherCloser
{
public:
    void operator()( co::MPIDispatcher * p )
    {
        if( p )
            p->close();
        delete p;
    }
};
}

#endif // CO_MPIDISPATCHER_H
