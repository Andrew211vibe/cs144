#include "reassembler.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  uint64_t push_len;

  if ( is_last_substring )
    fin_idx_ = first_index + data.length() - 1;

  if ( first_index == ack_ ) { // 如果等于ACK则立马push进byte_stream
    push_len = min( data.length(), output.available_capacity() );
    output.push( data );
    ack_ += push_len; // next expect byte
  } else if ( first_index > ack_ ) { // 如果大于ACK取capacity之内的字符
    std::string tmp = data.substr( 0, output.available_capacity() - first_index );
    for ( uint64_t i = first_index, j = 0; j < tmp.length(); j++, i++ )
      buf[i] = tmp[j];
  } else if ( first_index < ack_ && first_index + data.length() >= ack_ ) { // 部分已经ACK
    push_len = min( data.length() - ( ack_ - first_index ), output.available_capacity() );
    output.push( data.substr( ack_ - first_index, push_len ) );
    ack_ += push_len; // next expect byte
  }

  std::erase_if(buf, [&](const auto& pair){ return pair.first < ack_; });

  std::string temp;
  // while ( buf.find( ack_ ) != buf.end() && output.available_capacity() ) // buffer into ByteStream
  // {
  //   temp += buf[ ack_ ];
  //   buf.erase( buf.find( ack_++ ) );
  // }
  for ( auto p = buf.begin(); p != buf.end() && temp.length() < output.available_capacity(); ) {
    if (p->first == ack_) {
      temp += p->second;
      p = buf.erase(p);
      ++ack_;
    }
    else break;
  }
  output.push(temp);

  // clean up outdate buf
  // for ( auto i = buf.begin(); i != buf.end() && i->first < ack_; )
  //   i = buf.erase( i );

  if ( ack_ - 1 == fin_idx_ ) // 如果ACK等于FIN关闭byte_stream
    output.close();
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return buf.size();
}
