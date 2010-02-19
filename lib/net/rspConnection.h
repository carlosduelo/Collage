
/* Copyright (c) 2009, Cedric Stalder <cedric.stalder@gmail.com> 
 *               2009-2010, Stefan Eilemann <eile@equalizergraphics.com>
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

#ifndef EQNET_RSPCONNECTION_H
#define EQNET_RSPCONNECTION_H

#ifdef EQUALIZER_EXPORTS
   // We need to instantiate a Monitor< State > when compiling the library,
   // but we don't want to have <pthread.h> for a normal build, hence this hack
#  include <pthread.h>
#endif
#include <eq/net/connection.h>

#ifdef EQ_USE_BOOST
#include <eq/net/connectionSet.h> // member
#include <eq/net/types.h>

#include <eq/base/base.h>
#include <eq/base/buffer.h>  // member
#include <eq/base/lfQueue.h> // member
#include <eq/base/mtQueue.h> // member

#include "eventConnection.h" // member
#include "udpConnection.h"   // member

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
//#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

namespace eq
{
namespace net
{
    class ConnectionDescription;
    class RSPConnection;
    /** A rsp connection (Attn: only multicast usage implemented). */
    class RSPConnection : public Connection
    {

    public:
        /** Create a new rsp-based connection. */
        RSPConnection();
        virtual ~RSPConnection();

        virtual bool listen();
        void close();
        bool connect(){ return listen(); }

        virtual void acceptNB(){ EQASSERT( _state == STATE_LISTENING ); }

        virtual ConnectionPtr acceptSync();
        void readNB( void* buffer, const uint64_t bytes ){/* NOP */}
        int64_t readSync( void* buffer, const uint64_t bytes );
        int64_t write( const void* buffer, const uint64_t bytes );

        int64_t getSendRate() const { return _sendRate; }
        uint32_t getID() const { return _id; }
        
        /** @sa Connection::getNotifier */
        virtual Notifier getNotifier() const { return _event->getNotifier(); }
    
    private:
        typedef base::RefPtr< RSPConnection > RSPConnectionPtr;

        enum RSPState
        {
            RSP_STOPPED,
            RSP_ID,
            RSP_DISCOVER,
            RSP_RUNNING,
            RSP_ERROR
        };
        base::Monitor< RSPState > _rspState;

        /* manages RSP protocol directly using the udp connection */
        class Thread : public base::Thread
        {
        public: 
            Thread( RSPConnectionPtr connection )
                : _connection( connection ){}
            virtual ~Thread(){ _connection = 0; }
        protected:
            virtual void run();
            
        private:
            RSPConnectionPtr _connection;
        };

        enum DatagramType 
        { 
            DATA,      // the datagram contains data
            ACKREQ,    // ask for ack from all readers
            NACK,      // negative ack, request missing packets
            ACK,       // positive ack all data
            ID_HELLO,  // announce a new id
            ID_DENY,   // deny the id, already used
            ID_CONFIRM,// a new node is connected
            ID_EXIT,   // a node is disconnected
            COUNTNODE  // send to other the number of nodes which I have found
        };
        
        struct DatagramNode
        {
            uint16_t type;
            uint16_t connectionID;
        };

        struct DatagramCount
        {
            uint16_t type;
            uint16_t clientID;
            uint16_t numConnections;
        };

        struct DatagramAckRequest
        {
            uint16_t type;
            uint16_t writerID;
            uint16_t sequenceID;
        };
        
        struct DatagramNack
        {
            uint16_t       type;
            uint16_t       readerID;    // ID of the connection reader
            uint16_t       writerID;    // ID of the connection writer
            uint16_t       sequenceID;  // last datagram in write sequence
            uint16_t       count;       // number of NACK requests
        };

        struct DatagramAck
        {
            uint16_t        type;
            uint16_t        readerID;
            uint16_t        writerID;
            uint16_t        sequenceID;
        };

        struct DatagramData
        {
            uint16_t    type;
            uint16_t    size;
            uint16_t    writerID;
            uint16_t    sequenceID;
        };

        struct RepeatRequest
        {
            enum Type
            {
                DONE,
                ACKREQ,
                NACK
            };

            RepeatRequest() : type( NACK ), start( 0 ), end( 0 ) {}
            RepeatRequest( const Type& t ) 
                : type( t ), start( 0 ), end( 0 ) {}

            Type type;
            uint32_t start;
            uint32_t end;
        };
        
        // Buffer to send one NACK packet.
        eq::base::Bufferb _nackBuffer;

        // The buffer used by the write function in udp socket
        eq::base::Bufferb _sendBuffer;

        typedef std::vector< RSPConnectionPtr > RSPConnectionVector;
        // a link for all connection in the multicast network 
        RSPConnectionVector _children;

		// a link for all connection in the multicast network 
        RSPConnectionVector _childrenConnecting;
        
        uint16_t _id; //!< The identifier used to demultiplex multipe writers
        
        int32_t _timeouts;

        typedef base::RefPtr< EventConnection > EventConnectionPtr;
        EventConnectionPtr _event;

        boost::asio::io_service _ioService;
        boost::asio::ip::udp::socket* _connection;
        boost::asio::ip::udp::endpoint _writeAddr;
        boost::asio::ip::udp::endpoint _readAddr;
        boost::asio::deadline_timer _timer;
        eq::base::Clock _clock;
        size_t _allowedData;
        int64_t _sendRate;

        uint32_t         _numWriteAcks;
        Thread*          _thread;
        base::Lock       _mutexConnection;
        base::Lock       _mutexEvent;
        RSPConnectionPtr _parent;

        int32_t _ackReceived;  // sequence ID of last received/send ack
        int32_t _lastAck; // sequence ID of last confirmed ack
        bool _ackSend;    // ack exchange in progress

        typedef base::Bufferb Buffer;
        typedef std::vector< Buffer* > BufferVector;

        BufferVector _buffers;                   //!< Data buffers
        /** Empty read buffers (connected) or write buffers (listening) */
        base::LFQueue< Buffer* > _threadBuffers;
        /** Ready data buffers (connected) or empty write buffers (listening) */
        base::MTQueue< Buffer* > _appBuffers;

        Buffer _recvBuffer;                      //!< Receive (thread) buffer
        std::deque< Buffer* > _recvBuffers;      //!< out-of-order buffers

        Buffer* _readBuffer;                     //!< Read (app) buffer
        uint64_t _readBufferPos;                 //!< Current read index

        // write property part
        uint16_t _sequenceID; //!< the next usable (write) or expected (read)
        base::Buffer< Buffer* > _writeBuffers;    //!< Write buffers in flight
        std::deque< RepeatRequest > _repeatQueue; //!< nacks to repeat

        static uint32_t _payloadSize;
        static int32_t  _maxNAck;

        void _close();
        uint16_t _buildNewID();
        
        int32_t _handleWrite(); //!< @return time to call again
        void _finishWriteQueue();
        void _handleTimeout( const boost::system::error_code& error );
        void _handleData( const boost::system::error_code& error,
                          const size_t bytes );
        bool _handleDataDatagram( Buffer& buffer );
        bool _handleAck( const DatagramAck* ack );
        bool _handleNack( const DatagramNack* nack );
        bool _handleAckRequest( const DatagramAckRequest* ackRequest );

        /** @return true if we knew the correct number of connections. */
        bool _handleCountNode();

        Buffer* _newDataBuffer( Buffer& inBuffer );

        /* Run the reader thread */
        void _runThread();
        
        void _handleInitData();
        
        /** find the connection corresponding to the identifier */
        RSPConnectionPtr _findConnection( const uint16_t id );
        
        /** Analyze the current error and adapt the send rate */
        void _adaptSendRate( const size_t nPackets, const size_t errors );
        void _waitWritable( const uint64_t bytes );

        /** format and send a datagram count node */
        void _sendDatagramCountNode();

        void _handleRepeat();
        void _addRepeat( const uint16_t* repeatIDs, uint16_t size );

        /** format and send an ack request*/
        void _sendAckRequest( const uint16_t sequenceID );

        /** format and send a positive ack */
        void _sendAck( const uint16_t writerID, const uint16_t sequenceID );
        
        /** format and send a negative ack*/ 
        void _sendNack( const uint16_t toWriterID, const uint16_t sequenceID,
                        const uint16_t countNack, const uint16_t* nacks );
        
        void _checkNewID( const uint16_t id );

        /* add a new connection detected in the multicast network */
        void _addNewConnection( const uint16_t id );
        void _removeConnection( const uint16_t id );

        void _resetTimeout( uint32_t timeOut );

        CHECK_THREAD_DECLARE( _recvThread );
 
    };

    std::ostream& operator << ( std::ostream&, const RSPConnection& );
}
}

#endif //EQNET_RSPCONNECTION_H
#endif //EQ_USE_BOOST
