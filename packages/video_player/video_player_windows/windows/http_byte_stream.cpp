// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "http_byte_stream.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <windows.h>
#include <winhttp.h>

#include <cassert>
#include <future>
#include <iostream>

#pragma comment(lib, "WinHTTP")
#pragma comment(lib, "Mfuuid")
#pragma comment(lib, "Mfplat")

namespace video_player_windows {

// HttpConnection implementation.
bool HttpConnection::Open(const std::wstring& url,
                          const std::vector<std::wstring>& headers,
                          QWORD start_position) {
  URL_COMPONENTS url_comp = {sizeof(URL_COMPONENTS)};
  wchar_t host_name[1024];
  wchar_t url_path[1024];

  url_comp.lpszHostName = host_name;
  url_comp.lpszUrlPath = url_path;
  url_comp.dwHostNameLength = sizeof(host_name) / sizeof(wchar_t);
  url_comp.dwUrlPathLength = sizeof(url_path) / sizeof(wchar_t);

  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &url_comp)) {
    Close();
    return false;
  }

  session_ = WinHttpOpen(L"Flutter VideoPlayer/1.0",
                         WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session_) {
    Close();
    return false;
  }

  INTERNET_PORT port = url_comp.nPort;
  if (port == 0) {
    port = (url_comp.nScheme == INTERNET_SCHEME_HTTPS)
               ? INTERNET_DEFAULT_HTTPS_PORT
               : INTERNET_DEFAULT_HTTP_PORT;
  }

  connection_ = WinHttpConnect(session_, url_comp.lpszHostName, port, 0);
  if (!connection_) {
    Close();
    return false;
  }

  DWORD flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS)
                    ? WINHTTP_FLAG_SECURE
                    : 0;
  request_ = WinHttpOpenRequest(connection_, L"GET", url_comp.lpszUrlPath,
                                nullptr, WINHTTP_NO_REFERER, nullptr, flags);
  if (!request_) {
    Close();
    return false;
  }

  // Add Range header for seeking.
  if (start_position > 0) {
    wchar_t range_header[64];
    swprintf(range_header, sizeof(range_header) / sizeof(wchar_t),
             L"Range: bytes=%llu-", static_cast<unsigned long long>(start_position));
    WinHttpAddRequestHeaders(request_, range_header,
                             static_cast<DWORD>(wcslen(range_header)),
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  // Add custom headers.
  for (const auto& header : headers) {
    if (!WinHttpAddRequestHeaders(
            request_, header.c_str(), static_cast<DWORD>(header.length()),
            WINHTTP_ADDREQ_FLAG_ADD)) {
      Close();
      return false;
    }
  }

  if (!WinHttpSendRequest(request_, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    Close();
    return false;
  }

  if (!WinHttpReceiveResponse(request_, nullptr)) {
    Close();
    return false;
  }

  return true;
}

int HttpConnection::Read(BYTE* buffer, int size) {
  DWORD bytes_read = 0;
  if (WinHttpReadData(request_, buffer, static_cast<DWORD>(size),
                      &bytes_read) &&
      bytes_read > 0) {
    return static_cast<int>(bytes_read);
  }
  return -1;
}

void HttpConnection::Close() {
  if (request_) WinHttpCloseHandle(request_);
  if (connection_) WinHttpCloseHandle(connection_);
  if (session_) WinHttpCloseHandle(session_);
  request_ = connection_ = session_ = nullptr;
}

long HttpConnection::GetContentLength() {
  if (!request_) return -1;
  DWORD content_length = 0;
  DWORD length = sizeof(content_length);
  if (WinHttpQueryHeaders(
          request_,
          WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
          &content_length, &length, nullptr)) {
    return static_cast<long>(content_length);
  }
  return -1;
}

HttpConnection::~HttpConnection() { Close(); }

// Helper IUnknown for carrying read data through async results.
class DataUnknown : public IUnknown {
 public:
  explicit DataUnknown(ULONG data) : data_(data) {}
  ULONG GetData() const { return data_; }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == IID_IUnknown) {
      *ppv = static_cast<IUnknown*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  STDMETHODIMP_(ULONG) Release() override {
    ULONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) delete this;
    return count;
  }

 private:
  ULONG ref_count_ = 1;
  ULONG data_;
};

// HttpByteStream implementation.
HttpByteStream::HttpByteStream(const std::wstring& url,
                               const std::vector<std::wstring>& headers)
    : url_(url), headers_(headers) {}

HttpByteStream::~HttpByteStream() { Close(); }

STDMETHODIMP HttpByteStream::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) return E_POINTER;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFByteStream)) {
    *ppv = static_cast<IMFByteStream*>(this);
  } else if (riid == __uuidof(IMFByteStreamBuffering)) {
    *ppv = static_cast<IMFByteStreamBuffering*>(this);
  } else {
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) HttpByteStream::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) HttpByteStream::Release() {
  ULONG count = InterlockedDecrement(&ref_count_);
  if (count == 0) delete this;
  return count;
}

STDMETHODIMP HttpByteStream::GetCapabilities(DWORD* capabilities) {
  *capabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE |
                  MFBYTESTREAM_IS_REMOTE | MFBYTESTREAM_HAS_SLOW_SEEK;
  return S_OK;
}

STDMETHODIMP HttpByteStream::GetLength(QWORD* length) {
  if (file_size_ > 0) {
    *length = file_size_;
    return S_OK;
  }

  if (!connection_) OpenConnection(0);
  if (connection_) {
    file_size_ = connection_->GetContentLength();
    *length = file_size_;
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP HttpByteStream::SetLength(QWORD length) { return E_FAIL; }

STDMETHODIMP HttpByteStream::GetCurrentPosition(QWORD* position) {
  *position = position_;
  return S_OK;
}

STDMETHODIMP HttpByteStream::SetCurrentPosition(QWORD position) {
  if (position_ != position) {
    position_ = position;
    if (connection_) {
      delete connection_;
      connection_ = nullptr;
    }
  }
  return S_OK;
}

STDMETHODIMP HttpByteStream::IsEndOfStream(BOOL* end_of_stream) {
  QWORD size;
  if (!SUCCEEDED(GetLength(&size))) return E_FAIL;
  *end_of_stream = (position_ >= size);
  return S_OK;
}

STDMETHODIMP HttpByteStream::Read(BYTE* buffer, ULONG cb,
                                  ULONG* bytes_read) {
  if (!connection_) {
    if (!OpenConnection(position_)) return E_FAIL;
  }

  ULONG total_read = 0;
  while (cb > 0) {
    int len = connection_->Read(buffer, static_cast<int>(cb));
    if (len < 0) break;
    position_ += len;
    buffer += len;
    total_read += len;
    cb -= len;
    break;
  }
  *bytes_read = total_read;
  return S_OK;
}

STDMETHODIMP HttpByteStream::BeginRead(BYTE* buffer, ULONG cb,
                                       IMFAsyncCallback* callback,
                                       IUnknown* state) {
  if (!buffer || cb == 0 || !callback) return E_POINTER;

  // Capture the this pointer and ensure ref counting.
  AddRef();
  auto future = std::async(std::launch::async, [=]() {
    ULONG read_len = 0;
    Read(buffer, cb, &read_len);

    auto data = new DataUnknown(read_len);
    IMFAsyncResult* async_result = nullptr;
    MFCreateAsyncResult(data, callback, state, &async_result);
    data->Release();
    MFInvokeCallback(async_result);
    async_result->Release();
    Release();
  });

  return S_OK;
}

STDMETHODIMP HttpByteStream::EndRead(IMFAsyncResult* result,
                                     ULONG* bytes_read) {
  if (!result || !bytes_read) return E_POINTER;

  IUnknown* unknown = nullptr;
  result->GetObject(&unknown);
  if (!unknown) return E_FAIL;

  auto data = static_cast<DataUnknown*>(unknown);
  *bytes_read = data->GetData();
  unknown->Release();
  return S_OK;
}

STDMETHODIMP HttpByteStream::Write(const BYTE* buffer, ULONG cb,
                                   ULONG* bytes_written) {
  return E_FAIL;
}

STDMETHODIMP HttpByteStream::BeginWrite(const BYTE* buffer, ULONG cb,
                                        IMFAsyncCallback* callback,
                                        IUnknown* state) {
  return E_FAIL;
}

STDMETHODIMP HttpByteStream::EndWrite(IMFAsyncResult* result,
                                      ULONG* bytes_written) {
  return E_FAIL;
}

STDMETHODIMP HttpByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN origin,
                                  LONGLONG offset, DWORD flags,
                                  QWORD* current_position) {
  QWORD new_position;
  if (origin == msoBegin) {
    new_position = static_cast<QWORD>(offset);
  } else {
    new_position = position_ + static_cast<QWORD>(offset);
  }
  SetCurrentPosition(new_position);
  if (current_position) {
    *current_position = position_;
  }
  return S_OK;
}

STDMETHODIMP HttpByteStream::Flush() { return E_FAIL; }

STDMETHODIMP HttpByteStream::Close() {
  if (connection_) {
    delete connection_;
    connection_ = nullptr;
    return S_OK;
  }
  return S_OK;
}

STDMETHODIMP HttpByteStream::SetBufferingParams(
    MFBYTESTREAM_BUFFERING_PARAMS* params) {
  return S_OK;
}

STDMETHODIMP HttpByteStream::EnableBuffering(BOOL enable) { return S_OK; }

STDMETHODIMP HttpByteStream::StopBuffering() { return S_OK; }

bool HttpByteStream::OpenConnection(QWORD position) {
  delete connection_;
  connection_ = new HttpConnection();
  if (!connection_->Open(url_, headers_, position)) {
    delete connection_;
    connection_ = nullptr;
    return false;
  }
  position_ = position;
  return true;
}

}  // namespace video_player_windows
