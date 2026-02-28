// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef FLUTTER_PLUGIN_HTTP_BYTE_STREAM_H_
#define FLUTTER_PLUGIN_HTTP_BYTE_STREAM_H_

#include <mfidl.h>
#include <mfobjects.h>
#include <windows.h>
#include <winhttp.h>

#include <mutex>
#include <string>
#include <vector>

namespace video_player_windows {

// An HTTP connection helper for byte stream operations.
class HttpConnection {
 public:
  // Opens an HTTP connection to the given URL with optional headers.
  bool Open(const std::wstring& url,
            const std::vector<std::wstring>& headers,
            QWORD start_position = 0);
  // Reads data from the connection.
  int Read(BYTE* buffer, int size);
  // Closes the connection.
  void Close();
  // Returns the content length from the HTTP response.
  long GetContentLength();

  ~HttpConnection();

 private:
  HINTERNET session_ = nullptr;
  HINTERNET connection_ = nullptr;
  HINTERNET request_ = nullptr;
};

// An IMFByteStream implementation that reads from an HTTP(S) URL using
// WinHTTP. This allows the Media Foundation engine to play network videos
// with custom HTTP headers.
class HttpByteStream : public IMFByteStream,
                       public IMFByteStreamBuffering {
 public:
  HttpByteStream(const std::wstring& url,
                 const std::vector<std::wstring>& headers);
  ~HttpByteStream();

  // IUnknown methods.
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IMFByteStream methods.
  STDMETHODIMP GetCapabilities(DWORD* capabilities) override;
  STDMETHODIMP GetLength(QWORD* length) override;
  STDMETHODIMP SetLength(QWORD length) override;
  STDMETHODIMP GetCurrentPosition(QWORD* position) override;
  STDMETHODIMP SetCurrentPosition(QWORD position) override;
  STDMETHODIMP IsEndOfStream(BOOL* end_of_stream) override;
  STDMETHODIMP Read(BYTE* buffer, ULONG cb, ULONG* bytes_read) override;
  STDMETHODIMP BeginRead(BYTE* buffer, ULONG cb,
                         IMFAsyncCallback* callback,
                         IUnknown* state) override;
  STDMETHODIMP EndRead(IMFAsyncResult* result, ULONG* bytes_read) override;
  STDMETHODIMP Write(const BYTE* buffer, ULONG cb,
                     ULONG* bytes_written) override;
  STDMETHODIMP BeginWrite(const BYTE* buffer, ULONG cb,
                          IMFAsyncCallback* callback,
                          IUnknown* state) override;
  STDMETHODIMP EndWrite(IMFAsyncResult* result,
                        ULONG* bytes_written) override;
  STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN origin,
                    LONGLONG offset, DWORD flags,
                    QWORD* current_position) override;
  STDMETHODIMP Flush() override;
  STDMETHODIMP Close() override;

  // IMFByteStreamBuffering methods.
  STDMETHODIMP SetBufferingParams(
      MFBYTESTREAM_BUFFERING_PARAMS* params) override;
  STDMETHODIMP EnableBuffering(BOOL enable) override;
  STDMETHODIMP StopBuffering() override;

 private:
  // Opens or reopens the HTTP connection at the given position.
  bool OpenConnection(QWORD position);

  ULONG ref_count_ = 1;
  std::mutex mutex_;

  HttpConnection* connection_ = nullptr;
  std::wstring url_;
  std::vector<std::wstring> headers_;
  QWORD position_ = 0;
  QWORD file_size_ = 0;
};

}  // namespace video_player_windows

#endif  // FLUTTER_PLUGIN_HTTP_BYTE_STREAM_H_
