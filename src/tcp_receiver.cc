#include "tcp_receiver.hh"
#include <algorithm>
#include <string>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if ( !is_isn_set_ ) {
    // drop before SYN
    if ( !message.SYN )
      return;
    isn_ = message.seqno;
    is_isn_set_ = true;
  }

  uint64_t abs_ackno = inbound_stream.bytes_pushed() + 1;
  uint64_t cur_abs_seqno = message.seqno.unwrap( isn_, abs_ackno );
  uint64_t stream_index = cur_abs_seqno - 1 + ( message.SYN );
  string data = (string&)message.payload;
  reassembler.insert( stream_index, data, message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  uint16_t cap = uint16_t( std::min( inbound_stream.available_capacity(), uint64_t( UINT16_MAX ) ) );
  TCPReceiverMessage msg { nullopt, cap };
  Wrap32 ack( 0 );
  if ( is_isn_set_ ) {
    ack = isn_ + uint32_t( inbound_stream.bytes_pushed() + 1 );
    if ( inbound_stream.is_closed() )
      ack = ack + 1;
    msg.ackno = ack;
  }
  return msg;
}
