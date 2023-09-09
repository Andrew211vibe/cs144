#include "tcp_receiver.hh"
#include <algorithm>
#include <string>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  // 如果isn还没设置
  if ( !is_isn_set_ ) {
    // drop before SYN
    if ( !message.SYN ) // 如果这条message不是SYN则跳过
      return;
    isn_ = message.seqno; // 设置isn
    is_isn_set_ = true;
  }

  /*
   * 要获得stream index需要：
   * 1.将message.seqno转换成absolute seqno
   * 2.将absolute seqno转换成stream index
   */
  uint64_t abs_ackno = inbound_stream.bytes_pushed() + 1; // 获得first unassembled index
  // 1.根据first unassembled index设置message seqno获得absolute seqno
  uint64_t cur_abs_seqno = message.seqno.unwrap( isn_, abs_ackno );
  // 2.转换absolute seqno为stream index
  uint64_t stream_index = cur_abs_seqno - 1 + ( message.SYN );
  reassembler.insert( stream_index, message.payload.release(), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  uint16_t cap = uint16_t( std::min( inbound_stream.available_capacity(), uint64_t( UINT16_MAX ) ) );
  TCPReceiverMessage msg { nullopt, cap };
  Wrap32 ack( 0 );
  if ( is_isn_set_ ) {
    // 将stream index转换成absolute seqno
    // +1代表SYN，+inbound_stream.is_closed()为FIN
    ack = isn_ + uint32_t( inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed() );
    msg.ackno = ack;
  }
  return msg;
}
