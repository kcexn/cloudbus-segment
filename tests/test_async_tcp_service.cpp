/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Cloudbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cloudbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Cloudbus.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "segment/service/async_service.hpp"
#include "segment/service/async_tcp_service.hpp"

#include <arpa/inet.h>
#include <gtest/gtest.h>

using namespace cloudbus::service;

struct async_noop_service : public async_tcp_service<async_noop_service> {
  using Base = async_tcp_service<async_noop_service>;

  template <typename T>
  explicit async_noop_service(socket_address<T> address) : Base(address)
  {}

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::span<const std::byte> buf) -> void
  {
    msg.insert(msg.end(), buf.begin(), buf.end());
  }

  std::vector<std::byte> msg;
};

class AsyncTcpServiceTest : public ::testing::Test {};

TEST_F(AsyncTcpServiceTest, StartTest)
{
  using namespace io::socket;

  auto ctx = async_context{};
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  auto service = async_noop_service{addr};

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncTcpServiceTest, ReadTest)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = async_noop_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  {
    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, addr);
    ctx.poller.wait();

    auto buf = std::array<char, 1>{'x'};
    sendmsg(sock, socket_message{.buffers = buf}, 0);
    ctx.poller.wait();
  }

  EXPECT_EQ(static_cast<char>(service.msg.at(0)), 'x');
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}
// NOLINTEND
