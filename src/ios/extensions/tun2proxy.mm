// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#include "tun2proxy.h"

#import <NetworkExtension/NetworkExtension.h>
#include <fcntl.h>
#include <unistd.h>

#include <base/posix/eintr_wrapper.h>

struct Tun2Proxy_InitContext {
  __weak NEPacketTunnelFlow *packetFlow;
  int read_fd;
};

struct ReadPacketContext {
  NSData *data;
};

static const void* GetReadPacketContextData(void *context, void* packet) {
  NSData *data = reinterpret_cast<ReadPacketContext*>(packet)->data;

  return data.bytes;
}

static size_t GetReadPacketContextSize(void *context, void* packet) {
  NSData *data = reinterpret_cast<ReadPacketContext*>(packet)->data;
  return data.length;
}

static void FreeReadPacketContext(void *context, void* packet) {
  auto packet_context = reinterpret_cast<ReadPacketContext*>(packet);
  packet_context->data = nil;
  delete packet_context;
}

static NEPacket *packetFromData(NSData *data) {
  uint8_t version_ih = 0;
  [data getBytes:&version_ih length:sizeof(version_ih)];
  uint8_t version = version_ih >> 4;
  assert((version == 4 || version == 6) && "unsupported ip hdr");
  return [[NEPacket alloc] initWithData:data protocolFamily:version == 6 ? AF_INET6 : AF_INET];
}

static void WritePackets(void* context, void* const* packets, const size_t* packetLengths,
                         int packetsCount) {
  Tun2Proxy_InitContext *c = reinterpret_cast<Tun2Proxy_InitContext*>(context);
  NEPacketTunnelFlow* packetFlow = c->packetFlow;
  NSMutableArray *packetsArray = [NSMutableArray arrayWithCapacity:packetsCount];
  for (int i = 0; i < packetsCount; ++i) {
    NSData *data = [NSData dataWithBytesNoCopy:packets[i]
                                        length:packetLengths[i]
                                  freeWhenDone:NO];
    NEPacket *packet = packetFromData(data);
    [packetsArray addObject:packet];
  }
  if (![packetFlow writePacketObjects:packetsArray]) {
    NSLog(@"writePacketObjects failed");
  }
}

extern "C"
int tun2proxy_init(void* context,
                   int fd,
                   decltype(GetReadPacketContextData),
                   decltype(GetReadPacketContextSize),
                   decltype(FreeReadPacketContext),
                   decltype(WritePackets),
                   const char* proxy_url,
                   int tun_mtu,
                   int log_level,
                   int dns_over_tcp);

extern "C"
int tun2proxy_run();

extern "C"
int tun2proxy_destroy();

Tun2Proxy_InitContext* Tun2Proxy_Init(NEPacketTunnelFlow *packetFlow,
                                      const std::string& proxy_url,
                                      int tun_mtu,
                                      int log_level,
                                      bool dns_over_tcp) {
  auto context = new Tun2Proxy_InitContext;
  context->packetFlow = packetFlow;
  int fds[2] = {-1, -1};
  if (pipe(fds) < 0) {
    delete context;
    return nullptr;
  }
  fcntl(fds[0], F_SETFD, FD_CLOEXEC); // read end
  fcntl(fds[1], F_SETFD, FD_CLOEXEC); // write end
  fcntl(fds[0], F_SETFL, O_NONBLOCK | fcntl(fds[0], F_GETFL));
  context->read_fd = fds[1]; // save write end
  int ret = tun2proxy_init(context, fds[0],
                           GetReadPacketContextData, GetReadPacketContextSize,
                           FreeReadPacketContext, WritePackets,
                           proxy_url.c_str(),
                           tun_mtu,
                           log_level,
                           dns_over_tcp ? 1 : 0);
  if (ret < 0) {
    IGNORE_EINTR(close(fds[0]));
    IGNORE_EINTR(close(fds[1]));
    delete context;
    return nullptr;
  }
  return context;
}

int Tun2Proxy_Run(Tun2Proxy_InitContext* context) {
  return tun2proxy_run();
}

void Tun2Proxy_ForwardReadPackets(Tun2Proxy_InitContext* context, NSArray<NEPacket *> *packets) {
  for (NEPacket *packet in packets) {
    ReadPacketContext *p = new ReadPacketContext;
    p->data = packet.data;
    int ret = HANDLE_EINTR(write(context->read_fd, &p, sizeof(p)));
    if (ret < 0) {
      break;
    }
  }
}

void Tun2Proxy_Destroy(Tun2Proxy_InitContext* context) {
  tun2proxy_destroy();
  context->packetFlow = nil;
  IGNORE_EINTR(close(context->read_fd));
  delete context;
}
