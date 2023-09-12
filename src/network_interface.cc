#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto const ip_addr = next_hop.ipv4_numeric(); // 获取目的IP地址
  // 如果IP地址对应以太网地址映射已知则直接发送
  if (ip_to_ethernet_.contains(ip_addr))
  {
    // 将dgram序列化放入payload
    EthernetFrame ef {
      {ip_to_ethernet_[ip_addr].first, ethernet_address_, EthernetHeader::TYPE_IPv4},
      serialize(dgram)};
    // 立即发送
    outstanding_frames_.push(move(ef));
  } 
  else // 否则发送ARP消息请求IP到以太网地址映射,暂存IP数据报
  {
    // 如果next_hop对应ip已发送ARP则记录数据报到对应目的IP
    if (arp_time_.contains(ip_addr))
      inbound_datagrams_[ip_addr].push_back(dgram);
    else // 否则意味着还未向这个目的IP发送过ARP请求
    {
      // 更新已发送ARP表
      arp_time_.emplace(ip_addr, 0);
      // 缓存数据报等待ARP回复以发送
      inbound_datagrams_.insert( { ip_addr, { dgram } } );
      // 构建ARPMessage
      ARPMessage msg;
      msg.opcode = ARPMessage::OPCODE_REQUEST;
      msg.sender_ethernet_address = ethernet_address_;
      msg.sender_ip_address = ip_address_.ipv4_numeric();
      msg.target_ip_address = ip_addr;
      // 构建以太帧
      EthernetFrame ef {
        {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP},
        serialize(msg)};
      outstanding_frames_.push(move(ef)); // 压入正在发送帧
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 如果到达以太帧的目的地址不是广播和本机以太网地址,则忽略
  if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_) return nullopt;
  // 如果收到一个IPv4类型数据包
  if (frame.header.type == EthernetHeader::TYPE_IPv4)
  {
    InternetDatagram id;
    // 以InternetDatagram解析frame负载
    if (parse(id, frame.payload))
      return id;
  }
  else if (frame.header.type == EthernetHeader::TYPE_ARP) // 如果收到一个ARP包
  {
    ARPMessage msg;
    if (parse(msg, frame.payload)) // 以ARPMessage解析frame负载
    {
      ip_to_ethernet_[msg.sender_ip_address] = {msg.sender_ethernet_address, 0}; // 建立映射
      if (msg.target_ip_address == ip_address_.ipv4_numeric()) // 如果目的IP为本机IP
      {
        if (msg.opcode == msg.OPCODE_REPLY) // 如果是响应
        {
          uint32_t ip_addr = msg.sender_ip_address;
          // 收到响应，映射建立，重发之前未能发送的数据报
          // 因为通过键-目的地址IP 值-未发送数据报缓存list，直接将对应目的IP下list取出遍历发送即可
          auto const& datagrams = inbound_datagrams_[ip_addr];
          // 遍历等待列表重发IP数据报
          for (auto const& dgram : datagrams)
            send_datagram(dgram, Address::from_ipv4_numeric(ip_addr));
          // 清除已重发的数据报
          inbound_datagrams_.erase(ip_addr);
        }
        else if (msg.opcode == msg.OPCODE_REQUEST) // 收到请求包
        {
          // 根据本机IP和以太网地址构建响应ARPMessage
          ARPMessage response;
          response.sender_ip_address = ip_address_.ipv4_numeric();
          response.sender_ethernet_address = ethernet_address_;
          response.target_ethernet_address = msg.sender_ethernet_address;
          response.target_ip_address = msg.sender_ip_address;
          response.opcode = ARPMessage::OPCODE_REPLY;
          // 构建以太帧装载ARP消息
          EthernetFrame ef {
            {response.target_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP},
            serialize(response)};
          outstanding_frames_.push(move(ef)); // 帧入队返回响应
        }
      }
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  static constexpr size_t IP2ETHERNET_TTL = 30000;
  static constexpr size_t IP_ARP_TTL = 5000;
  // 到期ARP包
  for (auto iter = arp_time_.begin(); iter != arp_time_.end();)
  {
    iter->second += ms_since_last_tick; // 如果通过-=实现忽略了下溢问题
    if (iter->second >= IP_ARP_TTL) // 已到期的移除
      iter = arp_time_.erase(iter);
    else iter ++;
  }
  // 遍历更新ip到以太网地址映射表
  for (auto iter = ip_to_ethernet_.begin(); iter != ip_to_ethernet_.end();)
  {
    iter->second.second += ms_since_last_tick; // 如果通过-=实现忽略了下溢问题
    if (iter->second.second >= IP2ETHERNET_TTL) // 已超时的移除
      iter = ip_to_ethernet_.erase(iter);
    else
      iter ++;
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (outstanding_frames_.size()) // 如果正在发送帧队列中有未发送帧
  {
    auto ef = outstanding_frames_.front(); // 出队发送
    outstanding_frames_.pop();
    return ef;
  }
  return nullopt;
}
