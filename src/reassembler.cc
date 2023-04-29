#include "reassembler.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // cout << "START:\tbuffer: " << buf.size() << "\tack: " << ack_ << "\tlength: "
  // << data.length() << "\tpushed: " << output.bytes_pushed() << endl;
  // Your code here.
  uint64_t push_len;

  if ( is_last_substring )
    fin_idx_ = first_index + data.length() - 1;

  if ( first_index == ack_ ) // is expect byte
  {
    push_len = min( data.length(), output.available_capacity() );
    output.push( data.substr( 0, push_len ) );
    ack_ += push_len; // next expect byte
  } else if ( first_index > ack_ ) {
    std::string tmp = data.substr( 0, output.available_capacity() - first_index );
    for ( uint64_t i = first_index, j = 0; j < tmp.length(); j++, i++ )
      buf[i] = tmp[j];
  } else if ( first_index < ack_ && first_index + data.length() >= ack_ ) {
    push_len = min( data.length() - ( ack_ - first_index ), output.available_capacity() );
    output.push( data.substr( ack_ - first_index, push_len ) );
    ack_ += push_len; // next expect byte
  }

  while ( buf.find( ack_ ) != buf.end() && output.available_capacity() ) // buffer into ByteStream
  {
    output.push( buf[ack_] );
    buf.erase( buf.find( ack_++ ) );
  }

  for ( auto i = buf.begin(); i != buf.end() && i->first < ack_; ) // clean up outdate buf
    buf.erase( i++ );

  if ( ack_ - 1 == fin_idx_ )
    output.close();

  // cout << "END:\tbuffer: " << buf.size() << "\tack: " << ack_ << "\tlength: "
  // << data.length() << "\tpushed: " << output.bytes_pushed() << "\tFIN: " << fin_idx_ << endl;
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return buf.size();
}
