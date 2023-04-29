#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point + uint32_t( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  const constexpr uint64_t UINT32_RANGE = 1ULL << 32;
  uint32_t offset = raw_value_ - zero_point.raw_value_;

  if ( checkpoint > offset ) {
    uint64_t new_checkpoint = ( checkpoint - offset ) + ( UINT32_RANGE >> 1 );
    uint64_t wrap_time = new_checkpoint / UINT32_RANGE;
    return wrap_time * UINT32_RANGE + offset;
  } else
    return offset;
}
