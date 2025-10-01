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
 * @file async_service.hpp
 * @brief This file declares an asynchronous service.
 */
#pragma once
#ifndef CLOUDBUS_ASYNC_SERVICE_HPP
#define CLOUDBUS_ASYNC_SERVICE_HPP
#include "segment/detail/concepts.hpp"
#include "segment/detail/immovable.hpp"

#include <exec/async_scope.hpp>
#include <io.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
/** @brief This namespace holds definitions for bus services. */
namespace cloudbus::service {

/** @brief Data members for an asynchronous service context. */
struct async_context : detail::immovable {
  /** @brief Asynchronous scope type. */
  using async_scope = exec::async_scope;
  /** @brief The io multiplexer type. */
  using multiplexer = io::execution::poll_multiplexer;
  /** @brief The io triggers type. */
  using triggers = io::execution::basic_triggers<multiplexer>;
  /** @brief The signal mask type. */
  using signal_mask = std::uint64_t;

  /** @brief The event-loop interrupt */
  class interrupt_type {
  public:
    /** @brief Calls the underlying interrupt. */
    auto operator()() -> void;
    /** @brief Assigns a function to the underlying interrupt. */
    auto operator=(std::function<void()> func) noexcept -> interrupt_type &;
    /** @brief Tests to see if the interrupt has been assigned to. */
    explicit operator bool() const noexcept;

  private:
    /** @brief The underlying interrupt function. */
    std::function<void()> fn_;
    /** @brief A mutex for syncrhonization. */
    mutable std::mutex mtx_;
  };

  /** @brief An enum of all valid async context signals. */
  enum signals : std::uint8_t { terminate = 0, user1, END };

  /** @brief The asynchronous scope. */
  async_scope scope;
  /** @brief The poll triggers. */
  triggers poller;
  /** @brief A flag that determines whether the context has stopped. */
  std::atomic<bool> stopped;
  /** @brief The active signal mask. */
  std::atomic<signal_mask> sigmask;
  /** @brief The event loop interrupt. */
  interrupt_type interrupt;

  /**
   * @brief Sets the signal mask, then interrupts the service.
   * @param signum The signal to send. Must be in range of
   *               enum signals.
   */
  auto signal(int signum) -> void;
};

/**
 * @brief A threaded asynchronous service.
 *
 * This class runs the provided service in a separate thread
 * with an asynchronous context.
 *
 * @tparam Service The service to run.
 */
template <ServiceLike Service> class async_service : public async_context {
  using socket_dialog =
      io::socket::socket_dialog<io::execution::poll_multiplexer>;

  /**
   * @brief An interrupt service routine.
   *
   * This interrupts the running event loop in a thread so
   * that signals can be handled.
   *
   * @param scope The asynchronous scope.
   * @param socket The listening socket for interrupts.
   * @param handle A function that passes signals to the service
   *               signal handler.
   */
  template <typename Fn>
    requires std::is_invocable_r_v<bool, Fn>
  static auto isr(async_scope &scope, const socket_dialog &socket,
                  const Fn &handle) -> void;

  /** @brief Called when the async_service is stopped. */
  auto stop(int socket) noexcept -> void;

public:
  /** @brief Default constructor. */
  async_service() = default;
  /** @brief Deleted copy constructor. */
  async_service(const async_service &) = delete;
  /** @brief Deleted move constructor. */
  async_service(async_service &&) = delete;
  /** @brief Deleted copy assignment. */
  auto operator=(const async_service &) -> async_service & = delete;
  /** @brief Deleted move assignment. */
  auto operator=(async_service &&) -> async_service & = delete;

  /**
   * @brief Start the asynchronous service.
   *
   * This starts the provided service in a separate thread
   * with the provided asynchronous context.
   *
   * @param mtx A mutex for synchronization with the parent thread.
   * @param cvar A condition variable for synchronization with the parent
   *             thread.
   */
  auto start(std::mutex &mtx, std::condition_variable &cvar) -> void;

  /** @brief The destructor signals the thread before joining it. */
  ~async_service();

private:
  /** @brief The thread that serves the asynchronous service. */
  std::thread server_;
};

} // namespace cloudbus::service

#include "impl/async_service_impl.hpp" // IWYU pragma: export

#endif // CLOUDBUS_ASYNC_SERVICE_HPP
