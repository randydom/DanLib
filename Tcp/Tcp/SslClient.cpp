#include "SslClient.hpp"

using dl::tcp::SslClient;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
SslClient::SslClient(
  const std::string& Hostname,
  const unsigned Port,
  const unsigned NumberOfIoThreads,
  const unsigned NumberOfCallbackThreads)
: mIoService(),
  mCallbackService(),
  mResolver(mIoService),
  mpSession(nullptr),
  mTimer(mIoService),
  mHostname(Hostname),
  mPort(Port),
  mConnectionMutex(),
  mThreads(),
  mIsRunning(true),
  mpNullIoWork(std::make_unique<asio::io_service::work> (mIoService)),
  mpNullCallbackWork(std::make_unique<asio::io_service::work> (mCallbackService))
{
  StartWorkerThreads(mCallbackService, NumberOfCallbackThreads);

  Connect();

  StartWorkerThreads(mIoService, NumberOfIoThreads);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
SslClient::~SslClient()
{
  mIsRunning = false;

  mIoService.stop();

  mpNullIoWork.reset();

  mpNullCallbackWork.reset();

  for (auto& Thread : mThreads)
  {
    Thread.join();
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void dl::tcp::SslClient::StartWorkerThreads(
  asio::io_service& IoService,
  unsigned NumberOfThreads)
{
  for (unsigned i = 0u; i < NumberOfThreads; ++i)
  {
    mThreads.emplace_back([&IoService] { IoService.run(); });
  }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::Connect()
{
  mpSession = dl::tcp::SslSession::Create(mIoService, mCallbackService);

  mpSession->GetOnRxSignal().Connect(
    [this] (const std::string Bytes)
    {
      mSignalOnRx(Bytes);
    });

  mpSession->GetOnDisconnectSignal().Connect(
    [this] { StartConnect(); mSignalOnDisconnect();});

  StartConnect();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::StartConnect()
{
  asio::ip::tcp::resolver::query Query(mHostname, "https");

  mResolver.async_resolve(
    Query,
    [this]
    (const asio::error_code& Error, asio::ip::tcp::resolver::iterator iEndpoint)
    {
      OnResolve(Error, iEndpoint);
    });
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::OnResolve(
  const asio::error_code& Error,
  asio::ip::tcp::resolver::iterator iEndpoint)
{
  if (!Error)
  {
    mTimer.expires_from_now(std::chrono::seconds(2));

    asio::async_connect(
      mpSession->GetSocket(),
      iEndpoint,
      [this]
      (const asio::error_code& Error, asio::ip::tcp::resolver::iterator iEndpoint)
      {
        OnConnect(Error, iEndpoint);
      });

    mTimer.async_wait([this] (const asio::error_code& Error) { OnTimeout(Error); });
  }
  else
  {
    mSignalConnectionError(Error.message());
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::OnConnect(
  const asio::error_code& Error,
  asio::ip::tcp::resolver::iterator iEndpoint)
{
  if (!Error)
  {
    mpSession->Start();

    mSignalConnection();

    mTimer.cancel();
  }
  else if (iEndpoint != asio::ip::tcp::resolver::iterator())
  {
    asio::async_connect(
      mpSession->GetSocket(),
      iEndpoint,
      [this] (const asio::error_code& Error, asio::ip::tcp::resolver::iterator iEndpoint)
      {
        OnConnect(Error, ++iEndpoint);
      });
  }
  else
  {
    mSignalConnectionError(Error.message());
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::OnTimeout(const asio::error_code& Error)
{
  if (!Error)
  {
    Connect();
  }
  else if (Error != asio::error::operation_aborted)
  {
    mSignalConnectionError(Error.message());
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SslClient::Write(const std::string& Bytes)
{
  if (mpSession)
  {
    mpSession->Write(Bytes);
  }
}
