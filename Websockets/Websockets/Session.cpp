#include "Session.hpp"

#include <iostream>
#include <memory>

using dl::ws::Session;

std::atomic<unsigned long> Session::mCount(0ul);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
std::shared_ptr<Session> Session::Create(
  boost::asio::io_service& IoService,
  boost::asio::io_service& CallbackService)
{
  return std::shared_ptr<Session>(new Session(IoService, CallbackService));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Session::Session(
  boost::asio::io_service& IoService,
  boost::asio::io_service& CallbackService)
 : mSessionId(++mCount),
   mIoService(IoService),
   mCallbackService(CallbackService),
   mSocket(mIoService),
   mWebsocket(mSocket),
   mStrand(mIoService),
   mBuffer(),
   mIsSending(false)
{
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::Start()
{
  mWebsocket.async_accept(
    [this, pWeak = weak_from_this()] (const boost::system::error_code& Error)
    {
      if (auto pThis = pWeak.lock())
      {
        OnAccept(Error);
      }
    });
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::OnAccept(const boost::system::error_code& Error)
{
  if (Error)
  {
    mSignalOnDisconnect();

    if (Error == boost::beast::websocket::error::closed)
    {
      return;
    }

    mSignalError(Error, "On Accept");

    return;
  }


  DoRead();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::DoRead()
{
  mWebsocket.async_read(
    mBuffer,
    [this , pWeak = weak_from_this()] (const boost::system::error_code& Error, std::size_t BytesTransfered)
    {
      if (auto pThis = pWeak.lock())
      {
        OnRead(Error, BytesTransfered);
      }
    });
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::OnRead(const boost::system::error_code& Error, const size_t BytesTransfered)
{
  if (!Error)
  {
    auto Bytes = boost::beast::buffers_to_string(mBuffer.data());

    mCallbackService.post(
      [this, pWeak = weak_from_this(), Bytes = std::move(Bytes)]
      {
        if (auto pThis = pWeak.lock())
        {
          mSignalOnRx(Bytes);
        }
      });

    mBuffer.consume(mBuffer.size());

    DoRead();
  }
  else if (
    Error == boost::beast::websocket::error::closed ||
    Error == boost::asio::error::eof ||
    Error == boost::asio::error::connection_reset)
  {
    mCallbackService.post(
      [this, pWeak = weak_from_this()]
    {
      if (auto pThis = pWeak.lock())
      {
        mSignalOnDisconnect();
      }
    });
  }
  else
  {
    mCallbackService.post(
      [this, Error, pWeak = weak_from_this()]
      {
        if (auto pThis = pWeak.lock())
        {
          mSignalError(Error, "Read Error");
        }
      });
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::AsyncWrite()
{
  std::lock_guard Lock(mMutex);

  if (!mWriteQueue.empty() && !mIsSending)
  {
    DataType WriteDataType(DataType::eBinary);

    std::tie(mWriteBuffer, WriteDataType) = std::move(mWriteQueue.front());

    mIsSending = true;

    mWriteQueue.pop_front();

    mWebsocket.text(WriteDataType == DataType::eText);

    mWebsocket.async_write(
      boost::asio::buffer(mWriteBuffer),
      mStrand.wrap(
        [this, pThis = shared_from_this()]
        (const boost::system::error_code& Error, const size_t BytesTransfered)
        {
          if (!Error)
          {
            {
              std::lock_guard Lock(mMutex);

              mIsSending = false;
            }

            AsyncWrite();
          }
          else
          {
            mCallbackService.post(
              [=, pWeak = weak_from_this()]
              {
                if (auto pThis = pWeak.lock())
                {
                  mSignalError(Error, "Write Error");
                }
              });
          }
        }));
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Session::Write(const std::string& Bytes, dl::ws::DataType DataType)
{
  mIoService.post(mStrand.wrap(
    [this, DataType, Bytes = std::move(Bytes), pThis = shared_from_this()]
    {
      {
        std::lock_guard Lock(mMutex);

        mWriteQueue.emplace_back(std::move(Bytes), DataType);
      }

      AsyncWrite();
    }));
}
