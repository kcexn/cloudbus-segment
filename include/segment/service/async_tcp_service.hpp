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
 *                   std::span<const std::byte> buf) -> void
 *   {}
 * };
 * @endcode
 */
template <typename StreamHandler> class async_tcp_service {
private:
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
   */
  template <typename T>
  explicit async_tcp_service(socket_address<T> address) : address_{address}
  {}

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

  /** @brief handle signals. */
  auto signal_handler(int signum) const noexcept -> void
  {
    if (signum == terminate && stop_)
      stop_();
  };
  /** @brief Start the service on the context. */
  auto start(async_context &ctx) noexcept -> void
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
      auto sock = ctx.poller.emplace(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      sender auto connect = io::connect(sock, address_) |
                            then([sock](auto status) {}) |
                            upon_error([](const auto &error) {});
      ctx.scope.spawn(std::move(connect));
    };

    acceptor(ctx, ctx.poller.emplace(std::move(sock)));
  }

  /** @brief Accept new connections on a listening socket. */
  auto acceptor(async_context &ctx, const socket_dialog &socket) -> void
  {
    using namespace stdexec;
    if (ctx.scope.get_stop_token().stop_requested())
      return;

    sender auto accept = io::accept(socket) | then([&, socket](auto accepted) {
                           auto [dialog, addr] = std::move(accepted);
                           reader(ctx, dialog, std::make_shared<readbuf>());
                           acceptor(ctx, socket);
                         }) |
                         upon_error([](const auto &error) {});

    ctx.scope.spawn(std::move(accept));
  }

  /** @brief Read data from a connected socket. */
  auto reader(async_context &ctx, const socket_dialog &socket,
              std::shared_ptr<readbuf> rbuf) -> void
  {
    using namespace stdexec;
    using namespace io::socket;
    if (ctx.scope.get_stop_token().stop_requested())
      return;

    sender auto recvmsg =
        io::recvmsg(socket, rbuf->msg, 0) |
        then([&, socket, rbuf](auto len) mutable {
          if (!len)
            return;

          auto &handle = static_cast<StreamHandler &>(*this);
          handle(ctx, socket, std::span(rbuf->buffer.data(), len));

          reader(ctx, socket, std::move(rbuf));
        }) |
        upon_error([](const auto &error) {});

    ctx.scope.spawn(std::move(recvmsg));
  }
};

} // namespace cloudbus::service
#endif
