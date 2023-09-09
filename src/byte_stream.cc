#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Your code here.
  if ( available_capacity() == 0 || data.empty() )
    return;
  uint64_t write_size = min( data.length(), available_capacity() );
  if ( write_size < data.size() ) {
    data.resize( write_size );
  }
  buffer_.push( move( data ) );
  if ( buffer_.size() == 1 )
    buffer_view_ = buffer_.front();
  written_bytes_ += write_size;
  buffer_bytes_ += write_size;
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
  return capacity_ - buffer_bytes_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return written_bytes_;
}

string_view Reader::peek() const
{
  // Your code here.
  return buffer_view_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return buffer_.empty() && close_;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  if ( len > bytes_buffered() )
    return;

  while ( len > 0 ) {
    auto sz = buffer_view_.size();
    if ( len >= sz ) {
      len -= sz;
      buffer_.pop();
      buffer_view_ = buffer_.front();
      buffer_bytes_ -= sz;
      readed_bytes_ += sz;
    } else {
      buffer_view_.remove_prefix( len );
      buffer_bytes_ -= len;
      readed_bytes_ += len;
      return;
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return writer().bytes_pushed() - bytes_popped();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return readed_bytes_;
}
