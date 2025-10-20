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
#include "segment/segment_service.hpp"

#include <gtest/gtest.h>

#include <cassert>
#include <list>

#include <arpa/inet.h>
using namespace cloudbus::service;
using namespace cloudbus::segment;

class SegmentServiceTest : public ::testing::Test {};

TEST_F(SegmentServiceTest, StartTest)
{
  using namespace io::socket;

  auto list = std::list<async_service<segment_service>>{};
  auto &service = list.emplace_back();

  std::mutex mtx;
  std::condition_variable cvar;
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);

  service.start(mtx, cvar, addr);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.interrupt || service.stopped; });
  }
  ASSERT_TRUE(static_cast<bool>(service.interrupt));
  {
    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    ASSERT_EQ(connect(sock, addr), 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock, socket_message{.buffers = std::span(it, 1)}, 0),
                1);
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(buf[0], *it);
    }
  }
}
// NOLINTEND
