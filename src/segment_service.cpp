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
 * @file segment_service.cpp
 * @brief This file defines the segment service.
 */
#include "segment/segment_service.hpp"
namespace cloudbus::segment {

auto segment_service::initialize(const socket_handle &sock) -> void {}

auto segment_service::service(async_context &ctx, const socket_dialog &socket,
                              const std::shared_ptr<read_context> &rctx,
                              const socket_message &msg) -> void
{
  using namespace stdexec;

  sender auto sendmsg =
      io::sendmsg(socket, msg, 0) |
      then([&, socket, rctx](auto &&len) { reader(ctx, socket, rctx); }) |
      upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(sendmsg));
}

auto segment_service::operator()(async_context &ctx,
                                 const socket_dialog &socket,
                                 const std::shared_ptr<read_context> &rctx,
                                 std::span<const std::byte> buf) -> void
{
  service(ctx, socket, rctx, {.buffers = buf});
}
} // namespace cloudbus::segment
