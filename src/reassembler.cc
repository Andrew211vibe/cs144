#include "reassembler.hh"
#include <algorithm>
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if ( data.size() == 0 ) {
    if ( is_last_substring )
      output.close();
    return;
  }

  if ( output.available_capacity() == 0 )
    return; // 没容量缓存则直接返回

  const uint64_t end_idx = first_index + data.size(); // 计算出data的最后一位对应下标
  const uint64_t first_unacceptable_idx
    = first_unassembled_idx_ + output.available_capacity(); // 第一个不能接受的下标

  /*
   * data不在[first_unassemled_index, first_unacceptable_idx)
   * 也就是要么已经存在ByteStream中，要么超出available capacity，直接丢弃
   */
  if ( end_idx <= first_unassembled_idx_ || first_index >= first_unacceptable_idx )
    return;

  // 如果data部分数据超出了first_unacceptable_idx范围，则截断
  if ( end_idx > first_unacceptable_idx ) {
    data.resize( first_unacceptable_idx - first_index );
    is_last_substring = false; // 如果截断则不可能是last_substring
  }
  /*
   * 经过上述处理后，剩余substring必然不会超过first_unacceptable_idx
   * 情况1:first_index > first_unassembled_idx，此时前面有未知字节，保存进buf中
   * 情况2:first_index < fisrt_unassembled_idx，此时前面若干字节已存在于ByteStream中，需要截断无用前缀
   * 情况3:first_index = fisrt_unassembled_idx，马上传递给Wirter
   */
  if ( first_index > first_unassembled_idx_ ) {
    buffer_in( first_index, move( data ), is_last_substring );
    return;
  }
  if ( first_index < first_unassembled_idx_ ) {
    data = data.substr( first_unassembled_idx_ - first_index );
  }
  /*
   * 经过上述处理后，substring必定在[first_unassemled_index, first_unacceptable_idx)范围内
   * 即first_index == first_unassembled_idx_，可直接传递给Writer
   */
  first_unassembled_idx_ += data.size();
  output.push( move( data ) );

  if ( is_last_substring )
    output.close();

  // 如果buf不为空，且队首substring对应起始下标<=first_unassembled_idx_，代表可以传递给Writer
  if ( !buf.empty() && buf.begin()->first <= first_unassembled_idx_ )
    buffer_out( output );
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return unassembled_bytes_;
}

void Reassembler::buffer_in( uint64_t first_index, std::string data, bool is_last_substring )
{
  uint64_t ready_idx = first_index;
  const uint64_t end_idx = first_index + data.size();

  // 遍历缓存
  for ( auto iter = buf.begin(); iter != buf.end() && ready_idx < end_idx; ) {
    // 如果当前substring下标<=ready_idx，则更新起始索引为与substring下标+长度的最大值
    if ( iter->first <= ready_idx ) {
      ready_idx = max( ready_idx, iter->first + iter->second.size() );
      ++iter;
      continue;
    }
    // 如果准备索引==first_index且当前substring下标>=end_index
    if ( ready_idx == first_index && iter->first >= end_idx ) {
      unassembled_bytes_ += data.size();
      buf.emplace( iter, first_index, move( data ) );
      return;
    }

    // 选取连续substring的结束下标，并计算长度
    const uint64_t index = min( iter->first, end_idx );
    const uint64_t len = index - ready_idx;
    buf.emplace( iter, ready_idx, data.substr( ready_idx - first_index, len ) );
    unassembled_bytes_ += len;
    ready_idx = index;
  }

  if ( ready_idx < end_idx ) {
    unassembled_bytes_ += end_idx - ready_idx;
    buf.emplace_back( ready_idx, data.substr( ready_idx - first_index ) );
  }

  if ( is_last_substring )
    fin = true;
}

void Reassembler::buffer_out( Writer& output )
{
  // 遍历buf
  for ( auto iter = buf.begin(); iter != buf.end(); ) {
    if ( iter->first > first_unassembled_idx_ )
      break;

    const uint64_t end_idx = iter->first + iter->second.size();
    if ( end_idx <= first_unassembled_idx_ )
      unassembled_bytes_ -= iter->second.size();
    else {
      auto data = move( iter->second );
      unassembled_bytes_ -= data.size();
      if ( iter->first < first_unassembled_idx_ )
        data = data.substr( first_unassembled_idx_ - iter->first );
      first_unassembled_idx_ += data.size();
      output.push( move( data ) );
    }
    iter = buf.erase( iter );
  }
  if ( buf.empty() && fin )
    output.close();
}