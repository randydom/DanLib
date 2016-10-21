//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Dan Loman
// 2016-10-8
//
// Description:
//   This is a tcp client
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#pragma once
#include <Tcp/Session.hpp>

#include <Signal/Signal.hpp>

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/basic_waitable_timer.hpp>

//#include <optional>
#include <experimental/optional>

#include <thread>

namespace dl::tcp
{
  class Client
  {
    public:

      Client(
        const std::string& Hostname = "localhost",
        const unsigned Port = 8080,
        const unsigned NumberOfIoThreads = 2,
        const unsigned NumberOfCallbackThreads = 2);

      ~Client();

      Client(const Client& Other) = delete;

      Client& operator = (const Client& Rhs) = delete;

      void Write(const std::string& Bytes);

    private:

      void Connect();

      void StartWorkerThreads(
        asio::io_service& IoService,
        unsigned NumberOfThreads);

      void Entry();

      void OnResolve(
        const asio::error_code& Error,
        asio::ip::tcp::resolver::iterator iEndpoint);

      void OnConnect(
        const asio::error_code& Error,
        asio::ip::tcp::resolver::iterator iEndpoint);

      void OnTimeout(const asio::error_code& Error);

    private:

      asio::io_service mIoService;

      asio::io_service mCallbackService;

      asio::ip::tcp::resolver mResolver;

      std::experimental::optional<dl::tcp::Session> mSession;

      asio::basic_waitable_timer<std::chrono::system_clock> mTimer;

      std::string mHostname;

      unsigned mPort;

      std::mutex mConnectionMutex;

      std::vector<std::thread> mThreads;

      std::atomic<bool> mIsRunning;

      std::atomic<bool> mIsConnecting;

      std::unique_ptr<asio::io_service::work> mpNullIoWork;

      std::unique_ptr<asio::io_service::work> mpNullCallbackWork;

      dl::Signal<const std::string&> mSignalConnectionError;

      dl::Signal<const std::string&> mSignalOnRx;

  };
}
