#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
#include <algorithm>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return outstanding_bytes_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retransmit_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // 如果待发送队列为空则返回std::nullopt
  if ( inbound_segments_.empty() ) return nullopt;
  // 每当一个非空segment被发送,如果timer不在运行,启动timer
  if ( !timer_.is_running() ) timer_.start();
  // 从队列中取出待发送数据进行发送
  auto msg = inbound_segments_.front();
  inbound_segments_.pop();
  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  // 获取当前接收方窗口大小
  auto cur_win_size = win_size_ == 0 ? 1 : win_size_;
  // 填充接收方窗口
  while (outstanding_bytes_ < cur_win_size)
  {
    // 初始化TCPSenderMessage segment
    TCPSenderMessage msg;
    // 如果此前从未创建TCPSenderMessage,设置SYN
    if (!syn_)
      syn_ = msg.SYN = true;
    // 计算消息的序列号
    msg.seqno = Wrap32::wrap( absolute_seqno_, isn_ );

    // 计算最大可读取的字节数
    auto max_readable_size = min( cur_win_size - outstanding_bytes_, TCPConfig::MAX_PAYLOAD_SIZE );
    // 从outbound_stream读取max_readable_size字节数据到消息payload
    read(outbound_stream, max_readable_size, msg.payload);
    // 如果已经读取完毕,且之前未设置FIN,且接收窗口至少留有一位以放置FIN
    if (!fin_ && outbound_stream.is_finished() && outstanding_bytes_ + msg.sequence_length() < cur_win_size)
      fin_ = msg.FIN = true;
    auto len = msg.sequence_length();
    if ( len == 0 ) break; // 如果没有数据则退出循环
    outstanding_bytes_ += msg.sequence_length(); // 记录outstanding segment
    
    // 将新创建的TCPSenderMessage放入两个队列中
    inbound_segments_.push(msg);
    outstanding_segments_.push(msg);
    absolute_seqno_ += len; // 更新绝对序列号

    // 如果FIN已设置且outbound_stream中缓存字节数为0(代表已经读取全部数据)
    if (msg.FIN || outbound_stream.bytes_buffered() == 0) break;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 发送一个对应序列号的空消息
  auto const seqno = Wrap32::wrap( absolute_seqno_, isn_ );
  return { seqno, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 更新窗口大小
  win_size_ = msg.window_size;
  // 如果ackno不存在则直接丢弃这个信息
  if (msg.ackno.has_value())
  {
    // 将ackno转为绝对序列号(ackno调用unwrap函数,next_seqno_作为checkpoint)
    auto seqno = msg.ackno.value().unwrap(isn_, absolute_seqno_);
    // 如果收到的确认序列大于绝对序列号则返回
    if (seqno > absolute_seqno_) return;
    // 计算左右窗口边界
    while (outstanding_segments_.size())
    {
      auto outstanding = outstanding_segments_.front();
      // 如果seqno大于segment右边界则此segment已ACK,移出队列
      if ( outstanding.seqno.unwrap( isn_, absolute_seqno_ ) + outstanding.sequence_length() <= seqno ) 
      {
        outstanding_segments_.pop();
        outstanding_bytes_ -= outstanding.sequence_length(); // 更新移除后的outstanding segment字节数
        timer_.reset(); // 重置timer
        if ( outstanding_segments_.size() ) timer_.start(); // 如果还有outstanding,启动timer
        retransmit_cnt_ = 0; // 更新重传计数
      } else break;
    }
    if (outstanding_segments_.empty()) // 当所有outstanding都被ACK停止timer
      timer_.stop();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  timer_.tick(ms_since_last_tick);
  if ( timer_.is_expire() ) // 如果到期了
  {
    // 重传最早的outstanding
    inbound_segments_.push(outstanding_segments_.front());
    // 如果窗口大小不等于0
    if (win_size_ != 0)
    {
      ++ retransmit_cnt_; // 记录重传次数
      timer_.expo();
    }
    timer_.start();
  }
}
