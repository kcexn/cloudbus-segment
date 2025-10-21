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

#include <gtest/gtest.h>

#include <arpa/inet.h>
using namespace cloudbus::service;

static int error = 0;
int bind(int __fd, const struct sockaddr *addr, socklen_t __len)
{
  errno = static_cast<int>(std::errc::interrupted);
  error = errno;
  return -1;
}

class AsyncTcpServiceTest : public ::testing::Test {};

struct echo_block_service : public async_tcp_service<echo_block_service> {
  using Base = async_tcp_service<echo_block_service>;
  using socket_message = io::socket::socket_message<>;

  template <typename T>
  explicit echo_block_service(socket_address<T> address) : Base(address)
  {}

  bool initialized = false;
  auto initialize(const socket_handle &sock) -> std::error_code
  {
    if (initialized)
      return std::make_error_code(std::errc::invalid_argument);

    initialized = true;
    return {};
  }

  auto echo(async_context &ctx, const socket_dialog &socket,
            const std::shared_ptr<read_context> &rmsg,
            socket_message msg) -> void
  {
    using namespace io::socket;
    using namespace stdexec;

    sender auto sendmsg =
        io::sendmsg(socket, msg, 0) |
        then([&, socket, msg, rmsg](auto &&len) mutable {
          if (auto buffers = std::move(msg.buffers); buffers += len)
            return echo(ctx, socket, rmsg, {.buffers = buffers});

          reader(ctx, socket, std::move(rmsg));
        }) |
        upon_error([](auto &&error) {});

    ctx.scope.spawn(std::move(sendmsg));
  }

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rmsg,
                  std::span<const std::byte> buf) -> void
  {
    echo(ctx, socket, rmsg, {.buffers = buf});
  }
};

TEST_F(AsyncTcpServiceTest, BindError)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_block_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  EXPECT_TRUE(ctx.scope.get_stop_token().stop_requested());
  EXPECT_EQ(error, static_cast<int>(std::errc::interrupted));

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}
// NOLINTEND
