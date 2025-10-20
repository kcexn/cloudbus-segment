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
 * @file async_tcp_service_impl.hpp
 * @brief This file defines an asynchronous tcp service.
 */
#pragma once
#ifndef CLOUDBUS_ASYNC_TCP_SERVICE_IMPL_HPP
#define CLOUDBUS_ASYNC_TCP_SERVICE_IMPL_HPP
#include "segment/service/async_tcp_service.hpp"
namespace cloudbus::service {

template <typename TCPStreamHandler>
template <typename T>
async_tcp_service<TCPStreamHandler>::async_tcp_service(
    socket_address<T> address)
    : address_{address}
{}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::signal_handler(
    int signum) const noexcept -> void
{
  if (signum == terminate && stop_)
    stop_();
}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::start(async_context &ctx) noexcept
    -> void
{
  using namespace io;
  using namespace io::socket;

  auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  initialize_(sock);

  stop_ = [&] {
    using namespace stdexec;
    ctx.scope.request_stop();
    sender auto connect =
        io::connect(ctx.poller.emplace(AF_INET, SOCK_STREAM, IPPROTO_TCP),
                    address_) |
        then([](auto status) {}) | upon_error([](auto &&error) {});
    ctx.scope.spawn(std::move(connect));
  };

  acceptor(ctx, ctx.poller.emplace(std::move(sock)));
}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::acceptor(
    async_context &ctx, const socket_dialog &socket) -> void
{
  using namespace stdexec;
  if (ctx.scope.get_stop_token().stop_requested())
    return;

  sender auto accept = io::accept(socket) | then([&, socket](auto accepted) {
                         auto [dialog, addr] = std::move(accepted);
                         reader(ctx, dialog, std::make_shared<read_context>());
                         acceptor(ctx, socket);
                       }) |
                       upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(accept));
}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::reader(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rmsg) -> void
{
  using namespace stdexec;
  using namespace io::socket;
  if (ctx.scope.get_stop_token().stop_requested())
    return;

  sender auto recvmsg =
      io::recvmsg(socket, rmsg->msg, 0) |
      then([&, socket, rmsg](auto &&len) mutable {
        using size_type = std::size_t;
        if (!len)
          return;

        auto buf = std::span{rmsg->buffer.data(), static_cast<size_type>(len)};
        emit(ctx, socket, std::move(rmsg), buf);
      }) |
      upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(recvmsg));
}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::emit(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rmsg, std::span<const std::byte> buf) -> void
{
  auto &handle = static_cast<TCPStreamHandler &>(*this);
  handle(ctx, socket, std::move(rmsg), buf);
}

template <typename TCPStreamHandler>
auto async_tcp_service<TCPStreamHandler>::initialize_(
    const socket_handle &socket) -> void
{
  using namespace io;
  using namespace io::socket;

  if (auto reuse = socket_option<int>(1);
      setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reuse))
  {
    return; // GCOVR_EXCL_LINE
  }

  if constexpr (requires(TCPStreamHandler handler) {
                  handler.initialize(socket);
                })
  {
    auto &handler = static_cast<TCPStreamHandler &>(*this);
    handler.initialize(socket);
  }

  if (bind(socket, address_))
    return; // GCOVR_EXCL_LINE
  address_ = getsockname(socket, address_);

  if (listen(socket, SOMAXCONN))
    return; // GCOVR_EXCL_LINE
}

} // namespace cloudbus::service
#endif
