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

#include <gtest/gtest.h>

#include <condition_variable>
#include <list>
#include <mutex>

using namespace cloudbus::service;

int socketpair(int domain, int type, int __protocol, int __fds[2])
{
  return -1;
}

class AsyncServiceTest : public ::testing::Test {};

std::mutex test_mtx;
std::condition_variable test_cv;
static int test_signal = 0;
static int test_started = 0;
struct test_service {
  auto signal_handler(int signum) noexcept -> void
  {
    std::lock_guard lock{test_mtx};
    test_signal = signum;
    test_cv.notify_all();
  }
  auto start(async_context &ctx) noexcept -> void
  {
    std::lock_guard lock{test_mtx};
    test_started = 1;
    test_cv.notify_all();
  }
};

TEST_F(AsyncServiceTest, StartTest)
{
  auto list = std::list<async_service<test_service>>{};
  auto &service = list.emplace_back();
  ASSERT_FALSE(static_cast<bool>(service.interrupt));

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.interrupt || service.stopped; });
  }
  EXPECT_TRUE(service.stopped);
}
// NOLINTEND
