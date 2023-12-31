// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#include "ios/tun2proxy.h"

#import <NetworkExtension/NetworkExtension.h>

struct InitContent {
  __weak NEPacketTunnelFlow *packetFlow;
};

struct ReadPacketContent {
  NSArray<NEPacket *> *packets;
};

static size_t GetPacketCount(void *content, void* packets) {
  NSArray<NEPacket *> *array = reinterpret_cast<ReadPacketContent*>(packets)->packets;
  return [array count];
}

static const void* GetPacketDataAtIndex(void *content, void* packets, int index) {
  NSArray<NEPacket *> *array = reinterpret_cast<ReadPacketContent*>(packets)->packets;
  NEPacket *packet = array[index];
  uint32_t prefix = CFSwapInt32HostToBig((uint32_t)packet.protocolFamily);
  // Prepend data with network protocol. It should be done because on tun2proxy
  // uses uint32_t prefixes containing network protocol.
  NSMutableData *data = [[NSMutableData alloc] initWithCapacity:sizeof(prefix) + packet.data.length];
  [data appendBytes:&prefix length:sizeof(prefix)];
  [data appendData:packet.data];

  return data.bytes;
}

static size_t GetPacketSizeAtIndex(void *content, void* packets, int index) {
  NSArray<NEPacket *> *array = reinterpret_cast<ReadPacketContent*>(packets)->packets;
  return sizeof(uint32_t) + [[array[index] data] length];
}

static NEPacket *packetFromData(NSData *data) {
  // Get network protocol from prefix
  NSUInteger prefixSize = sizeof(uint32_t);

  if (data.length < prefixSize) {
    return nil;
  }

  uint32_t protocol = PF_UNSPEC;
  [data getBytes:&protocol length:prefixSize];
  protocol = CFSwapInt32BigToHost(protocol);

  NSRange range = NSMakeRange(prefixSize, data.length - prefixSize);
  NSData *packetData = [data subdataWithRange:range];

  return [[NEPacket alloc] initWithData:packetData protocolFamily:protocol];
}

static void WritePackets(void* content, void** packets, size_t* packetLengths,
                         int packetsCount) {
  InitContent *c = reinterpret_cast<InitContent*>(content);
  NEPacketTunnelFlow* packetFlow = c->packetFlow;;
  NSMutableArray *packetsArray = [NSMutableArray array];
  for (int i = 0; i < packetsCount; ++i) {
    NSData *data = [NSData dataWithBytes:packets[i] length:packetLengths[i]];
    NEPacket *packet = packetFromData(data);
    [packetsArray addObject:packet];
  }
  [packetFlow writePacketObjects:packetsArray];
}

extern "C"
void tun2proxy_init(void* content, decltype(WritePackets));

extern "C"
void tun2proxy_read_packets(void* packets, decltype(GetPacketCount),
                            decltype(GetPacketDataAtIndex),
                            decltype(GetPacketSizeAtIndex));

extern "C"
void* tun2proxy_destroy();

void Tun2Proxy_Init(NEPacketTunnelFlow *packetFlow) {
  InitContent *c = new InitContent;
  c->packetFlow = packetFlow;
  tun2proxy_init(c, WritePackets);
}

void Tun2Proxy_ForwardReadPackets(NSArray<NEPacket *> *packets) {
  ReadPacketContent p;
  p.packets = packets;
  tun2proxy_read_packets(&p, GetPacketCount, GetPacketDataAtIndex,
                         GetPacketSizeAtIndex);
}

void Tun2Proxy_Destroy() {
  InitContent *c = reinterpret_cast<InitContent*>(tun2proxy_destroy());
  delete c;
}
