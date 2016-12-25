#include "Responce.hpp"
#include <String/String.hpp>
#include <algorithm>

using dl::http::Responce;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Responce::Responce()
  : mBytes(),
    mStatus(Status::eUninitialized),
    mHeader(),
    mContentLength(0),
    mBody(),
    mMutex()
{
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Responce::Responce(const Responce& Other)
  : mBytes(Other.mBytes),
    mStatus(Other.mStatus),
    mHeader(Other.mHeader),
    mBody(Other.mBody),
    mMutex()
{
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool Responce::AddBytes(const std::string& Bytes)
{
  {
    std::lock_guard<std::mutex> Lock(mMutex);
    mBytes += Bytes;
  }

  if (mStatus == Status::eUninitialized)
  {
    ParseStatus();
  }

  if (mHeader.empty())
  {
    ParseHeader();
  }

  if (mBody.empty())
  {
    return ParseBody();
  }

  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
dl::http::Responce::Status Responce::GetStatus() const
{
  return mStatus;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Responce::SetStatus(const dl::http::Responce::Status Status)
{
  mStatus = Status;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const std::vector<std::string>& Responce::GetHeader() const
{
  return mHeader;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const std::string& Responce::GetBody() const
{
  return mBody;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Responce::ParseStatus()
{
  std::lock_guard<std::mutex> Lock(mMutex);

  auto iLineBreak = mBytes.find("\r\n");

  if (iLineBreak != std::string::npos)
  {
    mStatus = static_cast<Status>(atoi(mBytes.substr(8, iLineBreak - 8).c_str()));

    mBytes = mBytes.substr(iLineBreak + 2);
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Responce::ParseHeader()
{
  std::lock_guard<std::mutex> Lock(mMutex);
  auto iLineBreak = mBytes.find("\r\n\r\n");

  if (iLineBreak != std::string::npos)
  {
    mHeader = dl::Split(mBytes.substr(0, iLineBreak));

    ParseContentLength();

    mBytes = mBytes.substr(iLineBreak + 4);
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void Responce::ParseContentLength()
{
  auto iLine = std::find_if(
    mHeader.begin(),
    mHeader.end(),
    [] (const std::string& Line)
    {
      return Line.substr(0, 16) == "Content-Length: ";
    });;

  if (iLine == mHeader.end())
  {
    mContentLength = 1;
    mBytes += ' ';
  }
  else
  {
    mContentLength = std::atoi(iLine->substr(16).c_str());
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool Responce::ParseBody()
{
  std::lock_guard<std::mutex> Lock(mMutex);
  if (mBytes.size() >= mContentLength)
  {
    mBody = std::move(mBytes);

    return true;
  }
  return false;
}
