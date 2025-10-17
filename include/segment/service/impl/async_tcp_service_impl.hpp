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

template <typename StreamHandler>
template <typename T>
async_tcp_service<StreamHandler>::async_tcp_service(socket_address<T> address)
    : address_{address}
{}

template <typename StreamHandler>
auto async_tcp_service<StreamHandler>::signal_handler(int signum) const noexcept
    -> void
{
  if (signum == terminate && stop_)
    stop_();
}

template <typename StreamHandler>
auto async_tcp_service<StreamHandler>::start(async_context &ctx) noexcept
    -> void
{
  using namespace io;
  using namespace io::socket;

  auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  auto reuse = socket_option<int>(1);
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reuse))
    return; // GCOVR_EXCL_LINE

  if (bind(sock, address_))
    return; // GCOVR_EXCL_LINE
  address_ = getsockname(sock, address_);

  if (listen(sock, SOMAXCONN))
    return; // GCOVR_EXCL_LINE

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

template <typename StreamHandler>
auto async_tcp_service<StreamHandler>::acceptor(
    async_context &ctx, const socket_dialog &socket) -> void
{
  using namespace stdexec;
  if (ctx.scope.get_stop_token().stop_requested())
    return;

  sender auto accept = io::accept(socket) | then([&, socket](auto accepted) {
                         auto [dialog, addr] = std::move(accepted);
                         reader(ctx, dialog, std::make_shared<readbuf>());
                         acceptor(ctx, socket);
                       }) |
                       upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(accept));
}

template <typename StreamHandler>

auto async_tcp_service<StreamHandler>::reader(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<readbuf> rbuf) -> void
{
  using namespace stdexec;
  using namespace io::socket;
  if (ctx.scope.get_stop_token().stop_requested())
    return;

  sender auto recvmsg = io::recvmsg(socket, rbuf->msg, 0) |
                        then([&, socket, rbuf](auto &&len) mutable {
                          if (!len)
                            return;

                          auto &handle = static_cast<StreamHandler &>(*this);
                          auto span = std::span{rbuf->buffer.data(),
                                                static_cast<std::size_t>(len)};
                          auto blocked = handle(ctx, socket, rbuf, span);
                          if (!blocked)
                            return reader(ctx, socket, std::move(rbuf));
                        }) |
                        upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(recvmsg));
}

} // namespace cloudbus::service
#endif
