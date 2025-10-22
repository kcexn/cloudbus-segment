#include "segment/segment_service.hpp"

#include <condition_variable>
#include <csignal>
#include <thread>

using namespace net::service;
using namespace cloudbus::segment;

using service_type = async_service<segment_service>;

static constexpr std::uint16_t PORT = 8080;

static auto signal_mask() -> sigset_t *
{
  static auto set = sigset_t{};
  static sigset_t *setp = nullptr;
  static auto mtx = std::mutex{};

  if (auto lock = std::lock_guard{mtx}; !setp)
  {
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    setp = &set;
  }
  return setp;
}

static auto handle_signals(service_type &service) -> std::thread
{
  static const auto *sigmask = signal_mask();
  pthread_sigmask(SIG_BLOCK, sigmask, nullptr);

  return std::thread([&]() noexcept {
    auto stop_condition = [](int signal) { return signal == SIGTERM; };

    for (int signal = 0; !stop_condition(signal);)
    {
      using enum service_type::signals;

      sigwait(sigmask, &signal);

      if (signal == SIGTERM)
        service.signal(terminate);
    }
  });
}

auto main(int argc, char *argv[]) -> int
{
  using namespace io::socket;

  auto mtx = std::mutex{};
  auto cvar = std::condition_variable{};

  auto address = socket_address<sockaddr_in>{};
  address->sin_family = AF_INET;
  address->sin_port = htons(PORT);

  auto service = service_type{};
  auto sighandler = handle_signals(service);

  service.start(mtx, cvar, address);

  auto lock = std::unique_lock{mtx};
  cvar.wait(lock, [&] { return service.stopped.load(); });

  sighandler.join();
  return 0;
}
