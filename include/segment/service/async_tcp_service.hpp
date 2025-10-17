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
 * @file async_tcp_service.hpp
 * @brief This file declares an asynchronous tcp service.
 */
#pragma once
#ifndef CLOUDBUS_ASYNC_TCP_SERVICE_HPP
#define CLOUDBUS_ASYNC_TCP_SERVICE_HPP
#include "async_service.hpp"
namespace cloudbus::service {
/**
 * @brief A ServiceLike Async TCP Service.
 * @note The default constructor of async_tcp_service is private
 * so async_tcp_service can't be accidentally constructed without
 * a proper stream handler.
 * @details async_tcp_service is a CRTP class compliant with the ServiceLike
 * concept. It must be used with an inheriting CRTP specialization that
 * defines what the service should do with bytes it reads off the wire.
 * See `noop_service` below for an example of how to specialize
 * async_tcp_service.
 * @tparam StreamHandler The StreamHandler type that derives from
 * async_tcp_service.
 * @code
 * struct noop_service : public async_tcp_service<noop_service>
 * {
 *   using Base = async_tcp_service<noop_service>;
 *
 *   template <typename T>
 *   explicit noop_service(socket_address<T> address): Base(address)
 *   {}
 *
 *   auto operator()(async_context &ctx, const socket_dialog &socket,
 *                   const std::shared_ptr<readbuf> &rmsg,
 *                   std::span<const std::byte> buf) -> bool
 *   {
 *     static constexpr auto blocked = false;
 *     return blocked;
 *   }
 * };
 * @endcode
 */
template <typename StreamHandler> class async_tcp_service {
private:
  /** @brief Stop the service. */
  std::function<void()> stop_;

protected:
  /** @brief socket_address template type. */
  template <typename T> using socket_address = io::socket::socket_address<T>;

  /** @brief Default constructor. */
  async_tcp_service() = default;
  /**
   * @brief Socket address constructor.
   * @tparam T The socket address type.
   * @param address The service address to bind.
   */
  template <typename T> explicit async_tcp_service(socket_address<T> address);

  /** @brief Service address. */
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  socket_address<sockaddr_in> address_;

public:
  /** @brief The async scope type. */
  using async_scope = async_context::async_scope;
  /** @brief The io multiplexer type. */
  using multiplexer_type = async_context::multiplexer;
  /** @brief The socket dialog type. */
  using socket_dialog = io::socket::socket_dialog<multiplexer_type>;
  /** @brief Re-export the async_context signals. */
  using enum async_context::signals;

  /** @brief A read context. */
  struct readbuf {
    /** @brief The read buffer size. */
    static constexpr std::size_t BUFSIZE = 1024UL;
    /** @brief The read buffer type. */
    using buffer_type = std::array<std::byte, BUFSIZE>;
    /** @brief The socket message type. */
    using socket_message = io::socket::socket_message<>;

    /** @brief The read buffer. */
    buffer_type buffer{};
    /** @brief The read socket message. */
    socket_message msg{.buffers = buffer};
  };

  /**
   * @brief handle signals.
   * @param signum The signal number to handle.
   */
  auto signal_handler(int signum) const noexcept -> void;

  /**
   * @brief Start the service on the context.
   * @param ctx The async context to start the service on.
   */
  auto start(async_context &ctx) noexcept -> void;

  /**
   * @brief Accept new connections on a listening socket.
   * @param ctx The async context to start the acceptor on.
   * @param socket The socket to listen for connections on.
   */
  auto acceptor(async_context &ctx, const socket_dialog &socket) -> void;

  /**
   * @brief Read data from a connected socket.
   * @param ctx The async context to start the reader on.
   * @param socket the socket to read data from.
   * @param rbuf A shared pointer to a mutable read buffer.
   */
  auto reader(async_context &ctx, const socket_dialog &socket,
              std::shared_ptr<readbuf> rbuf) -> void;
};

} // namespace cloudbus::service

#include "impl/async_tcp_service_impl.hpp" // IWYU pragma: export
#endif
