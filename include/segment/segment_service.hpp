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
 * @file segment_service.hpp
 * @brief This file declares the cloudbus segment network service.
 */
#pragma once
#ifndef CLOUDBUS_SEGMENT_SERVICE_HPP
#define CLOUDBUS_SEGMENT_SERVICE_HPP
#include "service/async_tcp_service.hpp"
/** @namespace For cloudbus segment definitions. */
namespace cloudbus::segment {
/** @brief The service type to use. */
template <typename TCPStreamHandler>
using service_base = service::async_tcp_service<TCPStreamHandler>;

struct segment_service : public service_base<segment_service> {
  /** @brief The base class. */
  using Base = service_base<segment_service>;
  /** @brief The socket message type. */
  using socket_message = io::socket::socket_message<>;

  /**
   * @brief Constructs segment_service on the socket address.
   * @tparam T The type of the socket_address.
   * @param address The local IP address to bind to.
   */
  template <typename T>
  explicit segment_service(socket_address<T> address) noexcept : Base(address)
  {}

  auto initialize(const socket_handle &sock) -> void;

  auto service(async_context &ctx, const socket_dialog &socket,
               const std::shared_ptr<read_context> &rctx,
               const socket_message &msg) -> void;

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  const std::shared_ptr<read_context> &rctx,
                  std::span<const std::byte> buf) -> void;
};
} // namespace cloudbus::segment
#endif // CLOUDBUS_SEGMENT_SERVICE_HPP
