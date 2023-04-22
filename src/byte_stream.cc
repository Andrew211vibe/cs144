#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), queue_(), close_( false ), written_bytes_( 0 ), readed_bytes_( 0 ), error_( false )
{}

void Writer::push( string data )
{
  // Your code here.
  uint64_t write_size = min( data.length(), available_capacity() );
  written_bytes_ += write_size;
  for ( uint64_t i = 0; i < write_size; i++ )
    queue_.push( data[i] );
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

void Writer::set_error()
{
  // Your code here.
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - queue_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return written_bytes_;
}

string Reader::peek() const
{
  // Your code here.
  std::string s;
  if ( queue_.size() )
    s += queue_.front();
  return s;
}

bool Reader::is_finished() const
{
  // Your code here.
  return queue_.empty() && close_;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t read_size = min( len, queue_.size() );
  readed_bytes_ += read_size;
  for ( uint64_t i = 0; i < read_size; i++ )
    queue_.pop();
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return queue_.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return readed_bytes_;
}
