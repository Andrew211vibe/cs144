#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  forwarding_table_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

void Router::route() 
{
  // 检查每个网络接口是否收到数据包
  for (auto& curr_interface : interfaces_)
  {
    auto datagram = curr_interface.maybe_receive();
    if (datagram.has_value()) // 如果不为空,匹配转发表
    {
      auto& dgram = datagram.value();
      // 如果TTL为0或在递减后为0则直接drop,否则重新计算校验和并发送数据报
      if (dgram.header.ttl > 1)
      {
        dgram.header.ttl --; // 更新ttl
        dgram.header.compute_checksum(); // 重新计算校验和
        auto dst_ip = dgram.header.dst;

        // 遍历转发表计算最长前缀匹配
        pair<vector<Router::route_item_>::iterator, int> longest_prefix {forwarding_table_.end(), -1};
        for (auto route = forwarding_table_.begin(); route != forwarding_table_.end(); ++ route)
        {
          auto match_len = match_length(dst_ip, route->route_prefix_, route->prefix_length_);
          if (match_len > longest_prefix.second)
          {
            longest_prefix.second = match_len;
            longest_prefix.first = route;
          }
        }

        if (longest_prefix.first != forwarding_table_.end()) // 存在匹配
        {
          // 获取下一跳接口
          auto& target_interface = interface(longest_prefix.first->interface_num_);
          // 如果next_hop为空则发送到数据报目的地址,否则发送到next_hop地址
          target_interface.send_datagram(dgram, 
            longest_prefix.first->next_hop_.value_or(Address::from_ipv4_numeric(dst_ip)));
        }
      }
    }
  }
}

int Router::match_length(uint32_t src, uint32_t dst, uint8_t len)
{
  if (len == 0) return 0;
  if (len > 32) return -1;

  const uint8_t left = 32U - len;
  return (src >> left) == (dst >> left) ? len : -1;
}