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

/**
 * @file async_service_impl.hpp
 * @brief This file defines the asynchronous service.
 */
#pragma once
#ifndef CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
#define CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
#include "segment/detail/with_lock.hpp"
#include "segment/service/async_service.hpp"

#include <stdexec/execution.hpp>
namespace cloudbus::service {

template <ServiceLike Service>
template <typename Fn>
  requires std::is_invocable_r_v<bool, Fn>
auto async_service<Service>::isr(async_scope &scope,
                                 const socket_dialog &socket,
                                 const Fn &handle) -> void
{
  using namespace io::socket;
  using namespace stdexec;

  static constexpr std::size_t bufsize = 256;
  static auto buffer = std::array<char, bufsize>{};
  static auto msg = socket_message{.buffers = buffer};

  auto recvmsg = io::recvmsg(socket, msg, 0) | then([=, &scope](auto len) {
                   if (handle())
                     isr(scope, socket, handle);
                 }) |
                 upon_error([](auto &&err) noexcept {}) |
                 upon_stopped([]() noexcept {});
  scope.spawn(std::move(recvmsg));
}

template <ServiceLike Service>
auto async_service<Service>::stop(int socket) noexcept -> void
{
  interrupt = std::function<void()>{};
  stopped = true;
  io::socket::close(socket);
}

template <ServiceLike Service>
auto async_service<Service>::start(std::mutex &mtx,
                                   std::condition_variable &cvar) -> void
{
  server_ = std::thread([&]() noexcept {
    using namespace detail;
    auto service = Service{};
    auto isockets = std::array<int, 2>{};

    with_lock(std::unique_lock{mtx}, [&]() noexcept {
      using namespace io::socket;
      if (::socketpair(AF_UNIX, SOCK_STREAM, 0, isockets.data()))
        return stop(isockets[1]); // GCOVR_EXCL_LINE

      interrupt = [socket = isockets[1]]() noexcept {
        static auto message = std::array<char, 1>{};
        io::sendmsg(socket, socket_message{.buffers = message}, 0);
      };
    });
    cvar.notify_all();
    if (stopped)
      return; // GCOVR_EXCL_LINE

    isr(scope, poller.emplace(isockets[0]), [&]() noexcept {
      auto sigmask_ = sigmask.exchange(0);
      for (int signum = 0; auto mask = (sigmask_ >> signum); ++signum)
      {
        if (mask & (1 << 0))
          service.signal_handler(signum);
      }
      return !(sigmask_ & (1 << terminate));
    });

    service.start(static_cast<async_context &>(*this));
    while (poller.wait());

    with_lock(std::unique_lock{mtx},
              [&]() noexcept { return stop(isockets[1]); });
    cvar.notify_all();
  });
}

template <ServiceLike Service> async_service<Service>::~async_service()
{
  signal(terminate);
  server_.join();
}
} // namespace cloudbus::service
#endif // CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
