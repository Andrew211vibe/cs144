#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <queue>

/*
 * 实现一个重传timer类
 * 1.支持指数增长
 * 2.启动timer
 * 3.是否到期
 * 4.是否正在运行
 * 5.重置RTO为初始值
 * 6.停止timer
 * 7.计时tick
 */
class Timer
{
private:
  enum STATUS{ IDLE, RUNNING, EXPIRE }; // 空闲、运行中、到期
  int status { IDLE }; // timer状态
  uint64_t initial_RTO_ms_; // RTO初始值
  uint64_t RTO_; // 当前RTO
  uint64_t sum_time_ {0}; // 经历时间
public:
  Timer(uint64_t initial_RTO_ms) : initial_RTO_ms_(initial_RTO_ms), RTO_(initial_RTO_ms) {}
  // 启动计时
  void start()
  {
    status = RUNNING;
    sum_time_ = 0;
  }
  // 指数增长
  void expo() { RTO_ *= 2; }
  // 重置：将RTO重置为初始值
  void reset() { RTO_ = initial_RTO_ms_; }
  // timer是否在运行
  bool is_running() { return status == RUNNING; }
  // timer是否到期
  bool is_expire() { return status == EXPIRE; }
  // 停止timer
  void stop() { status = IDLE; }
  // 计时函数
  void tick(uint64_t ms_since_last_tick) 
  {
    if (status == RUNNING) sum_time_ += ms_since_last_tick;
    if (sum_time_ >= RTO_) status = EXPIRE;
  }
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_; // 初始RTO
  uint64_t absolute_seqno_ {0}; // 绝对序列号
  uint64_t win_size_ {1}; // 接收端窗口大小
  uint64_t outstanding_bytes_ {0}; // 记录outstanding segment字节数
  uint64_t retransmit_cnt_ {0}; // 连续重传计数
  bool syn_ {false};
  bool fin_ {false};

  std::queue<TCPSenderMessage> inbound_segments_ {}; // 待发送消息队列
  std::queue<TCPSenderMessage> outstanding_segments_ {}; // 正在发送的消息队列
  Timer timer_ { initial_RTO_ms_ };
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
