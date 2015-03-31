/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

***** HeadSocket v0.1, created by Jan Pinter **** Minimalistic header only WebSocket server implementation in C++ *****
                    Sources: https://github.com/P-i-N/HeadSocket, contact: Pinter.Jan@gmail.com
                     PUBLIC DOMAIN - no warranty implied or offered, use this at your own risk

-----------------------------------------------------------------------------------------------------------------------

Usage:
- use this as a regular header file, but in EXACTLY one of your C++ files (ie. main.cpp) you must define
  HEADSOCKET_IMPLEMENTATION beforehand, like this:

    #define HEADSOCKET_IMPLEMENTATION
    #include <HeadSocket.h>
    
-----------------------------------------------------------------------------------------------------------------------

Version history:

  = 0.1
    - TcpServer, TcpClient, CustomTcpServer<T>, WebSocketClient

/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>

#ifndef HEADSOCKET_PLATFORM_OVERRIDE
#ifdef _WIN32
#define HEADSOCKET_PLATFORM_WINDOWS
#elif __ANDROID__
#define HEADSOCKET_PLATFORM_ANDROID
#elif __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC
#define HEADSOCKET_PLATFORM_MAC
#endif
#elif __linux
#define HEADSOCKET_PLATFORM_NIX
#elif __unix
#define HEADSOCKET_PLATFORM_NIX
#elif __posix
#define HEADSOCKET_PLATFORM_NIX
#else
#error Unsupported platform!
#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Forward declarations */
namespace std { class thread; }

namespace headsocket {

/* Forward declarations */
struct ConnectionParams;
class SHA1;
class BaseTcpServer;
class BaseTcpClient;
class TcpClient;
class AsyncTcpClient;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SHA1
{
public:
  typedef uint32_t Digest32[5];
  typedef uint8_t Digest8[20];

  inline static uint32_t rotateLeft(uint32_t value, size_t count) { return (value << count) ^ (value >> (32 - count)); }

  SHA1();
  ~SHA1() { }

  void processByte(uint8_t octet);
  void processBlock(const void *start, const void *end);
  void processBytes(const void *data, size_t len);
  const uint32_t *getDigest(Digest32 digest);
  const uint8_t *getDigestBytes(Digest8 digest);

private:
  void processBlock();

  Digest32 _digest;
  uint8_t _block[64];
  size_t _blockByteIndex;
  size_t _byteCount;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Encoding
{
  static size_t base64(const void *src, size_t srcLength, void *dst, size_t dstLength);
  static size_t xor32(uint32_t key, void *ptr, size_t length);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Endian
{
  static uint16_t swap16(uint16_t value);
  static uint32_t swap32(uint32_t value);
  static uint64_t swap64(uint64_t value);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Handshake
{
  static bool webSocket(ConnectionParams *params);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class Opcode { Continuation = 0x00, Text = 0x01, Binary = 0x02, ConnectionClose = 0x08, Ping = 0x09, Pong = 0x0A };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DataBlock
{
  Opcode opcode;
  bool isCompleted;
  size_t offset;
  size_t length;
  DataBlock(Opcode op, size_t off) : opcode(op), isCompleted(false), offset(off), length(0) { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Object
{
protected:
  virtual void addRef() const = 0;
  virtual void release() const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADSOCKET_SERVER_CTOR(className, baseClassName) \
  className(int port): baseClassName(port) { }

class BaseTcpServer
{
public:
  virtual ~BaseTcpServer();

  void stop();
  bool isRunning() const;
  void disconnect(BaseTcpClient *client);

protected:
  BaseTcpServer(int port);

  virtual bool clientHandshake(headsocket::ConnectionParams *params) { return true; }
  virtual BaseTcpClient *clientAccept(headsocket::ConnectionParams *params) { return nullptr; }
  virtual void clientConnected(headsocket::BaseTcpClient *client) { }
  virtual void clientDisconnected(headsocket::BaseTcpClient *client) { }

  struct BaseTcpServerImpl *_p;

private:
  template <typename T> friend class Enumerator;

  void removeAllDisconnected() const;

  size_t addRefClients() const;
  void releaseClients() const;

  BaseTcpClient *getClient(size_t index) const;
  size_t getNumClients() const;

  void acceptThread();
  void disconnectThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class Handle
{
public:
  Handle(T *ptr) : _ptr(ptr) { if (_ptr) _ptr->addRef(); }
  Handle(const Handle &rp) : _ptr(rp._ptr) { if (_ptr) _ptr->addRef(); }
  Handle(Handle &&rp) : _ptr(rp._ptr) { rp._ptr = nullptr; }
  ~Handle() { if (_ptr) _ptr->release(); }

  Handle &operator=(T *ptr) { assign(ptr); return *this; }
  Handle &operator=(const Handle &rp) { assign(rp._ptr); return *this; }
  Handle &operator=(Handle &&rp)
  {
    if (_ptr != rp._ptr) { if (_ptr) _ptr->release(); _ptr = rp._ptr; rp._ptr = nullptr; }
    return *this;
  }
  
  operator T *() const { return _ptr; }
  T &operator*() const { return *_ptr; }
  T *operator->() const { return _ptr; }
  T *get() const { return _ptr; }
  bool operator!() const { return _ptr == nullptr; }

private:
  void assign(T *ptr)
  {
    if (_ptr == ptr) return;
    T* tmp_ptr = _ptr; _ptr = ptr;
    if (_ptr) _ptr->addRef();
    if (tmp_ptr) tmp_ptr->release();
  }

  template <class Other> friend class Handle;
  T *_ptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class Enumerator
{
public:
  Enumerator(const BaseTcpServer *server): _server(server) { _count = _server->addRefClients(); }
  ~Enumerator() { _server->releaseClients(); }

  const BaseTcpServer *getServer() const { return _server; }
  size_t getCount() const { return _count; }

  struct Iterator
  {
    Enumerator *enumerator;
    size_t index;

    Iterator(Enumerator *e, size_t i): enumerator(e), index(i) { }

    bool operator==(const Iterator &iter) const { return iter.index == index && iter.enumerator == enumerator; }
    bool operator!=(const Iterator &iter) const { return iter.index != index || iter.enumerator != enumerator; }
    Iterator &operator++() { ++index; return *this; }
    T *operator*() const { return reinterpret_cast<T *>(enumerator->getServer()->getClient(index)); }
  };

  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, _count); }

private:
  const BaseTcpServer *_server;
  size_t _count;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class TcpServer : public BaseTcpServer
{
public:
  typedef BaseTcpServer Base;

  TcpServer(int port) : Base(port) { }
  virtual ~TcpServer() { }

  Enumerator<T> enumerateClients() const { return Enumerator<T>(this); }

protected:
  virtual void clientConnected(T *client) { }
  virtual void clientDisconnected(T *client) { }

  BaseTcpClient *clientAccept(ConnectionParams *params) override
  {
    T *newClient = new T(this, params);
    if (!newClient->isConnected()) { delete newClient; newClient = nullptr; }
    return newClient;
  }

private:
  void clientConnected(headsocket::BaseTcpClient *client) { clientConnected(reinterpret_cast<T *>(client)); }
  void clientDisconnected(headsocket::BaseTcpClient *client) { clientDisconnected(reinterpret_cast<T *>(client)); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADSOCKET_CLIENT_CTOR(className, baseClassName) \
  className(const char *address, int port): baseClassName(address, port) { } \
  className(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params): baseClassName(server, params) { }

class BaseTcpClient : public Object
{
public:
  static const size_t InvalidOperation = (size_t)(-1);

  virtual ~BaseTcpClient();

  void disconnect();
  bool isConnected() const;

  BaseTcpServer *getServer() const;
  size_t getID() const;

protected:
  BaseTcpClient(const char *address, int port);
  BaseTcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);

  struct BaseTcpClientImpl *_p;

private:
  friend class BaseTcpServer;
  template <typename T> friend class Handle;
  template <typename T> friend class Enumerator;

  void addRef() const override;
  void release() const override;
  size_t getNumRefs() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TcpClient : public BaseTcpClient
{
public:
  typedef BaseTcpClient Base;

  TcpClient(const char *address, int port);
  TcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);
  virtual ~TcpClient();

  size_t write(const void *ptr, size_t length);
  bool forceWrite(const void *ptr, size_t length);
  size_t read(void *ptr, size_t length);
  size_t readLine(void *ptr, size_t length);
  bool forceRead(void *ptr, size_t length);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class AsyncTcpClient : public BaseTcpClient
{
public:
  typedef BaseTcpClient Base;

  AsyncTcpClient(const char *address, int port);
  AsyncTcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);
  virtual ~AsyncTcpClient();

  void pushData(const void *ptr, size_t length, Opcode op = Opcode::Binary);
  void pushData(const char *text);
  size_t peekData(Opcode *op = nullptr) const;
  size_t popData(void *ptr, size_t length);

protected:
  virtual void initAsyncThreads();
  virtual size_t asyncWriteHandler(uint8_t *ptr, size_t length);
  virtual size_t asyncReadHandler(uint8_t *ptr, size_t length);
  virtual bool asyncReceivedData(const headsocket::DataBlock &db, uint8_t *ptr, size_t length);

  void killThreads();

  struct AsyncTcpClientImpl *_ap;

private:
  void writeThread();
  void readThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WebSocketClient : public AsyncTcpClient
{
public:
  static const size_t FrameSizeLimit = 128 * 1024;

  typedef AsyncTcpClient Base;

  WebSocketClient(const char *address, int port);
  WebSocketClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);
  virtual ~WebSocketClient();

  struct FrameHeader
  {
    bool fin;
    Opcode opcode;
    bool masked;
    size_t payloadLength;
    uint32_t maskingKey;
  };

protected:
  size_t asyncWriteHandler(uint8_t *ptr, size_t length) override;
  size_t asyncReadHandler(uint8_t *ptr, size_t length) override;

private:
  size_t parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header);
  size_t writeFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header);

  size_t _payloadSize = 0;
  FrameHeader _currentHeader;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class WebSocketServer : public TcpServer<T>
{
public:
  typedef TcpServer<T> Base;

  WebSocketServer(int port): Base(port) { }

protected:
  bool clientHandshake(ConnectionParams *params) override { return Handshake::webSocket(params); }
};

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_IMPLEMENTATION
#ifndef HEADSOCKET_IS_IMPLEMENTED
#define HEADSOCKET_IS_IMPLEMENTED

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <map>

#ifdef HEADSOCKET_PLATFORM_WINDOWS
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

#define HEADSOCKET_LOCK_SUFFIX(var, suffix) std::lock_guard<decltype(var)> __scopeLock##suffix(var);
#define HEADSOCKET_LOCK_SUFFIX2(var, suffix) HEADSOCKET_LOCK_SUFFIX(var, suffix)
#define HEADSOCKET_LOCK(var) HEADSOCKET_LOCK_SUFFIX2(var, __LINE__)

namespace headsocket {

struct ConnectionParams
{
  SOCKET clientSocket;
  sockaddr_in from;
  size_t id;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CriticalSection
{
  mutable std::atomic_bool consumerLock;

  CriticalSection() { consumerLock = false; }
  void lock() const { while (consumerLock.exchange(true)); }
  void unlock() const { consumerLock = false; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename M = CriticalSection>
struct LockableValue : M
{
  T value;
  T *operator->() { return &value; }
  const T *operator->() const { return &value; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Semaphore
{
  mutable std::atomic_size_t count;
  mutable std::mutex mutex;
  mutable std::condition_variable cv;

  Semaphore() { count = 0; }

  void lock() const
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]()->bool { return count > 0; });
    lock.release();
  }
  void unlock() { mutex.unlock(); }
  void notify() { { std::lock_guard<std::mutex> lock(mutex); ++count; } cv.notify_one(); }
  void consume() const { if (count) --count; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DataBlockBuffer
{
  std::vector<DataBlock> blocks;
  std::vector<uint8_t> buffer;

  DataBlockBuffer() { buffer.reserve(65536); }
  DataBlock &blockBegin(Opcode op) { blocks.emplace_back(op, buffer.size()); return blocks.back(); }
  DataBlock &blockEnd() { blocks.back().isCompleted = true; return blocks.back(); }
  void blockRemove()
  {
    if (blocks.empty()) return;
    buffer.resize(blocks.back().offset);
    blocks.pop_back();
  }
  void writeData(const void *ptr, size_t length)
  {
    buffer.resize(buffer.size() + length);
    memcpy(buffer.data() + buffer.size() - length, reinterpret_cast<const char *>(ptr), length);
    blocks.back().length += length;
  }
  size_t readData(void *ptr, size_t length)
  {
    if (!ptr || !length || blocks.empty() || !blocks.front().isCompleted) return 0;
    DataBlock &db = blocks.front();
    size_t result = db.length >= length ? length : db.length;
    memcpy(ptr, buffer.data() + db.offset, result);
    buffer.erase(buffer.begin(), buffer.begin() + result);
    if (!(db.length -= result)) blocks.erase(blocks.begin()); else blocks.front().opcode = Opcode::Continuation;
    for (auto &block : blocks) if (block.offset > db.offset) block.offset -= result;
    return result;
  }
  size_t peekData(Opcode *op = nullptr) const
  {
    if (blocks.empty() || !blocks.front().isCompleted) return 0;
    if (op) *op = blocks.front().opcode;
    return blocks.front().length;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_PLATFORM_WINDOWS
void setThreadName(const char *name)
{
  const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
  typedef struct tagTHREADNAME_INFO
  {
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
  } THREADNAME_INFO;
#pragma pack(pop)

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = -1;
  info.dwFlags = 0;

  __try { RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info); }
  __except (EXCEPTION_EXECUTE_HANDLER) { }
}
#else
void setThreadName(const char *name) { }
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
SHA1::SHA1(): _blockByteIndex(0), _byteCount(0)
{
  uint32_t *d = _digest;
  *d++ = 0x67452301; *d++ = 0xEFCDAB89; *d++ = 0x98BADCFE; *d++ = 0x10325476;*d++ = 0xC3D2E1F0;
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processByte(uint8_t octet)
{
  _block[_blockByteIndex++] = octet;
  ++_byteCount;

  if (_blockByteIndex == 64)
  {
    _blockByteIndex = 0;
    processBlock();
  }
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBlock(const void *start, const void *end)
{
  const uint8_t *begin = static_cast<const uint8_t *>(start);
  while (begin != end) processByte(*begin++);
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBytes(const void *data, size_t len) { processBlock(data, static_cast<const uint8_t *>(data) + len); }

//---------------------------------------------------------------------------------------------------------------------
const uint32_t *SHA1::getDigest(Digest32 digest)
{
  size_t bitCount = _byteCount * 8;
  processByte(0x80);
  if (_blockByteIndex > 56)
  {
    while (_blockByteIndex != 0) processByte(0);
    while (_blockByteIndex < 56) processByte(0);
  }
  else
    while (_blockByteIndex < 56) processByte(0);

  processByte(0); processByte(0); processByte(0); processByte(0);
  for (int i = 24; i >= 0; i -= 8) processByte(static_cast<unsigned char>((bitCount >> i) & 0xFF));

  memcpy(digest, _digest, 5 * sizeof(uint32_t));
  return digest;
}

//---------------------------------------------------------------------------------------------------------------------
const uint8_t *SHA1::getDigestBytes(Digest8 digest)
{
  Digest32 d32; getDigest(d32);
  size_t s[] = { 24, 16, 8, 0 };

  for (size_t i = 0, j = 0; i < 20; ++i, j = i % 4) digest[i] = ((d32[i >> 2] >> s[j]) & 0xFF);
  
  return digest;
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBlock()
{
  uint32_t w[80], s[] = { 24, 16, 8, 0 };

  for (size_t i = 0, j = 0; i < 64; ++i, j = i % 4) w[i/4] = j ? (w[i/4] | (_block[i] << s[j])) : (_block[i] << s[j]);
  for (size_t i = 16; i < 80; i++) w[i] = rotateLeft((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);
  Digest32 dig = { _digest[0], _digest[1], _digest[2], _digest[3], _digest[4] };

  for (size_t f, k, i = 0; i < 80; ++i)
  {
    if (i < 20) f = (dig[1] & dig[2]) | (~dig[1] & dig[3]), k = 0x5A827999;
    else if (i < 40) f = dig[1] ^ dig[2] ^ dig[3], k = 0x6ED9EBA1;
    else if (i < 60) f = (dig[1] & dig[2]) | (dig[1] & dig[3]) | (dig[2] & dig[3]), k = 0x8F1BBCDC;
    else f = dig[1] ^ dig[2] ^ dig[3], k = 0xCA62C1D6;

    uint32_t temp = rotateLeft(dig[0], 5) + f + dig[4] + k + w[i];
    dig[4] = dig[3]; dig[3] = dig[2];
    dig[2] = rotateLeft(dig[1], 30);
    dig[1] = dig[0]; dig[0] = temp;
  }

  for (size_t i = 0; i < 5; ++i) _digest[i] += dig[i];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
size_t Encoding::base64(const void *src, size_t srcLength, void *dst, size_t dstLength)
{
  if (!src || !srcLength || !dst || !dstLength) return 0;

  static const char *encodingTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static size_t modTable[] = { 0, 2, 1 }, result = 4 * ((srcLength + 2) / 3);

  if (result <= dstLength - 1)
  {
    const uint8_t *input = reinterpret_cast<const uint8_t *>(src);
    uint8_t *output = reinterpret_cast<uint8_t *>(dst);

    for (size_t i = 0, j = 0, triplet = 0; i < srcLength; triplet = 0)
    {
      for (size_t k = 0; k < 3; ++k) triplet = (triplet << 8) | (i < srcLength ? (uint8_t)input[i++] : 0);
      for (size_t k = 4; k--; ) output[j++] = encodingTable[(triplet >> k * 6) & 0x3F];
    }

    for (size_t i = 0; i < modTable[srcLength % 3]; i++) output[result - 1 - i] = '=';
    output[result] = 0;
  }

  return result;
}

//---------------------------------------------------------------------------------------------------------------------
size_t Encoding::xor32(uint32_t key, void *ptr, size_t length)
{
  uint8_t *data = reinterpret_cast<uint8_t *>(ptr);
  uint8_t *mask = reinterpret_cast<uint8_t *>(&key);

  for (size_t i = 0; i < length; ++i, ++data) *data = (*data) ^ mask[i % 4];

  return length;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
uint16_t Endian::swap16(uint16_t x) { return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8); }

//---------------------------------------------------------------------------------------------------------------------
uint32_t Endian::swap32(uint32_t x)
{
  return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);
}

//---------------------------------------------------------------------------------------------------------------------
uint64_t Endian::swap64(uint64_t x)
{
  return
    ((x & 0x00000000000000FFULL) << 56) | ((x & 0x000000000000FF00ULL) << 40) | ((x & 0x0000000000FF0000ULL) << 24) |
    ((x & 0x00000000FF000000ULL) << 8)  | ((x & 0x000000FF00000000ULL) >> 8)  | ((x & 0x0000FF0000000000ULL) >> 24) |
    ((x & 0x00FF000000000000ULL) >> 40) | ((x & 0xFF00000000000000ULL) >> 56);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t socketReadLine(SOCKET socket, void *ptr, size_t length)
{
  if (!ptr || !length) return 0;
  size_t result = 0;
  while (result < length - 1)
  {
    char ch;
    int r = recv(socket, &ch, 1, 0);
    if (!r || r == SOCKET_ERROR) return 0;

    if (r != 1 || ch == '\n') break;
    if (ch != '\r') reinterpret_cast<char *>(ptr)[result++] = ch;
  }
  reinterpret_cast<char *>(ptr)[result++] = 0;
  return result;
}

bool Handshake::webSocket(ConnectionParams *params)
{
  std::string key;
  char lineBuffer[256];
  while (true)
  {
    size_t result = socketReadLine(params->clientSocket, lineBuffer, 256);
    if (result <= 1) break;
    if (!memcmp(lineBuffer, "Sec-WebSocket-Key: ", 19)) key = lineBuffer + 19;
  }

  if (key.empty()) return false;

  key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  SHA1 sha; sha.processBytes(key.c_str(), key.length());
  SHA1::Digest8 digest; sha.getDigestBytes(digest);
  Encoding::base64(digest, 20, lineBuffer, 256);

  std::string response =
    "HTTP/1.1 101 Switching Protocols\nUpgrade: websocket\nConnection: Upgrade\nSec-WebSocket-Accept: ";
  response += lineBuffer;
  response += "\n\n";

  return send(params->clientSocket, response.c_str(), response.length(), 0) == response.length();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct BaseTcpServerImpl
{
  std::atomic_bool isRunning;
  std::atomic_bool disconnectThreadQuit;
  sockaddr_in local;
  LockableValue<std::vector<Handle<BaseTcpClient>>> connections;
  Semaphore disconnectSemaphore;
  int port = 0;
  SOCKET serverSocket = INVALID_SOCKET;
  std::thread *acceptThread = nullptr;
  std::thread *disconnectThread = nullptr;
  size_t nextClientID = 1;

  BaseTcpServerImpl() { isRunning = false; disconnectThreadQuit = false; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer::BaseTcpServer(int port) : _p(new BaseTcpServerImpl())
{
#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSADATA wsaData;
  WSAStartup(0x101, &wsaData);
#endif

  _p->local.sin_family = AF_INET;
  _p->local.sin_addr.s_addr = INADDR_ANY;
  _p->local.sin_port = htons(static_cast<unsigned short>(port));

  _p->serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (bind(_p->serverSocket, (sockaddr *)&_p->local, sizeof(_p->local)) != 0) return;
  if (listen(_p->serverSocket, 8)) return;

  _p->isRunning = true;
  _p->port = port;
  _p->acceptThread = new std::thread(std::bind(&BaseTcpServer::acceptThread, this));
  _p->disconnectThread = new std::thread(std::bind(&BaseTcpServer::disconnectThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer::~BaseTcpServer()
{
  stop();

#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSACleanup();
#endif

  delete _p;
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::stop()
{
  if (_p->isRunning.exchange(false))
  {
    closesocket(_p->serverSocket);
    { Enumerator<BaseTcpClient> e(this); for (auto client : e) client->disconnect(); }

    if (_p->acceptThread)
    {
      _p->acceptThread->join();
      delete _p->acceptThread; _p->acceptThread = nullptr;
    }

    if (_p->disconnectThread)
    {
      _p->disconnectThreadQuit = true;
      _p->disconnectSemaphore.notify();
      _p->disconnectThread->join();
      delete _p->disconnectThread; _p->disconnectThread = nullptr;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpServer::isRunning() const { return _p->isRunning; }

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::disconnect(BaseTcpClient *client)
{
  if (client)
  {
    clientDisconnected(client);
    _p->disconnectSemaphore.notify();
  }
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient *BaseTcpServer::getClient(size_t index) const
{
  HEADSOCKET_LOCK(_p->connections);
  return index < _p->connections->size() ? _p->connections->at(index) : nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpServer::getNumClients() const
{
  HEADSOCKET_LOCK(_p->connections);
  return _p->connections->size();
}

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpServer::addRefClients() const
{
  HEADSOCKET_LOCK(_p->connections);
  for (auto client : _p->connections.value) client->addRef();
  return _p->connections->size();
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::releaseClients() const
{
  HEADSOCKET_LOCK(_p->connections);
  for (auto client : _p->connections.value) client->release();
  removeAllDisconnected();
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::removeAllDisconnected() const
{
  size_t i = 0;
  while (i < _p->connections->size())
  {
    BaseTcpClient *client = _p->connections.value[i];
    if (!client->isConnected() && client->getNumRefs() == 1)
      _p->connections->erase(_p->connections->begin() + i);
    else
      ++i;
  }
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::acceptThread()
{
  setThreadName("BaseTcpServer::acceptThread");
  while (_p->isRunning)
  {
    ConnectionParams params;
    params.clientSocket = accept(_p->serverSocket, (struct sockaddr *)&params.from, NULL);
    params.id = _p->nextClientID++; if (!_p->nextClientID) ++_p->nextClientID;
    if (!_p->isRunning) break;

    if (params.clientSocket != INVALID_SOCKET)
    {
      bool failed = false;
      if (clientHandshake(&params))
      {
        HEADSOCKET_LOCK(_p->connections);
        if (BaseTcpClient *newClient = clientAccept(&params))
        { _p->connections->push_back(newClient); clientConnected(newClient); } else failed = true;
      }
      else failed = true;
      if (failed) { closesocket(params.clientSocket); --_p->nextClientID; if (!_p->nextClientID) --_p->nextClientID; }
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::disconnectThread()
{
  setThreadName("BaseTcpServer::disconnectThread");
  while (!_p->disconnectThreadQuit)
  {
    {
      HEADSOCKET_LOCK(_p->disconnectSemaphore);
      HEADSOCKET_LOCK(_p->connections);

      removeAllDisconnected();
      _p->disconnectSemaphore.consume();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct BaseTcpClientImpl
{
  std::atomic_int refCount;
  std::atomic_bool isConnected;
  sockaddr_in from;
  size_t id = 0;
  BaseTcpServer *server = nullptr;
  SOCKET clientSocket = INVALID_SOCKET;
  std::string address = "";
  int port = 0;

  BaseTcpClientImpl() { refCount = 0; isConnected = false; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::BaseTcpClient(const char *address, int port)
  : _p(new BaseTcpClientImpl())
{
  struct addrinfo *result = NULL, *ptr = NULL, hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char buff[16];
  sprintf_s(buff, "%d", port);

  if (getaddrinfo(address, buff, &hints, &result))
    return;

  for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
  {
    _p->clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (_p->clientSocket == INVALID_SOCKET)
      return;

    if (connect(_p->clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
    {
      closesocket(_p->clientSocket);
      _p->clientSocket = INVALID_SOCKET;
      continue;
    }

    break;
  }

  freeaddrinfo(result);

  if (_p->clientSocket == INVALID_SOCKET)
    return;

  _p->address = address;
  _p->port = port;
  _p->isConnected = true;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::BaseTcpClient(BaseTcpServer *server, ConnectionParams *params)
  : _p(new BaseTcpClientImpl())
{
  _p->server = server;
  _p->clientSocket = params->clientSocket;
  _p->from = params->from;
  _p->id = params->id;
  _p->isConnected = true;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::~BaseTcpClient()
{
  disconnect();
  delete _p;
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpClient::addRef() const { ++_p->refCount; }

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpClient::release() const { if (!(--_p->refCount)) delete this; }

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpClient::getNumRefs() const { return _p->refCount; }

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpClient::disconnect()
{
  if (_p->isConnected.exchange(false))
  {
    if (_p->clientSocket != INVALID_SOCKET)
    {
      closesocket(_p->clientSocket);
      _p->clientSocket = INVALID_SOCKET;
    }

    if (_p->server)
      _p->server->disconnect(this);
  }
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpClient::isConnected() const { return _p->isConnected; }

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer *BaseTcpClient::getServer() const { return _p->server; }

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpClient::getID() const { return _p->id; }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(const char *address, int port): Base(address, port)
{

}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(BaseTcpServer *server, ConnectionParams *params): Base(server, params)
{

}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::write(const void *ptr, size_t length)
{
  if (!ptr || !length) return 0;
  int result = send(_p->clientSocket, (const char *)ptr, length, 0);
  if (!result || result == SOCKET_ERROR) return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceWrite(const void *ptr, size_t length)
{
  if (!ptr) return true;

  const char *chPtr = (const char *)ptr;

  while (length)
  {
    int result = send(_p->clientSocket, chPtr, length, 0);
    if (!result || result == SOCKET_ERROR) return false;

    length -= (size_t)result;
    chPtr += result;
  }

  return true;
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::read(void *ptr, size_t length)
{
  if (!ptr || !length) return 0;
  int result = recv(_p->clientSocket, (char *)ptr, length, 0);
  if (!result || result == SOCKET_ERROR) return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::readLine(void *ptr, size_t length)
{
  if (!ptr || !length) return 0;

  size_t result = 0;
  while (result < length - 1)
  {
    char ch;
    int r = recv(_p->clientSocket, &ch, 1, 0);
    if (!r || r == SOCKET_ERROR) return 0;
    
    if (r != 1 || ch == '\n') break;
    if (ch != '\r') reinterpret_cast<char *>(ptr)[result++] = ch;
  }

  reinterpret_cast<char *>(ptr)[result++] = 0;
  return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceRead(void *ptr, size_t length)
{
  if (!ptr) return true;

  char *chPtr = (char *)ptr;
  while (length)
  {
    int result = recv(_p->clientSocket, chPtr, length, 0);
    if (!result || result == SOCKET_ERROR) return false;

    length -= (size_t)result; chPtr += result;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct AsyncTcpClientImpl
{
  Semaphore writeSemaphore;
  LockableValue<DataBlockBuffer> writeBlocks;
  LockableValue<DataBlockBuffer> readBlocks;
  std::thread *writeThread = nullptr;
  std::thread *readThread = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::AsyncTcpClient(const char *address, int port): Base(address, port), _ap(new AsyncTcpClientImpl())
{
  initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::AsyncTcpClient(BaseTcpServer *server, ConnectionParams *params): Base(server, params)
  , _ap(new AsyncTcpClientImpl())
{
  initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::~AsyncTcpClient()
{
  disconnect();

  _ap->writeSemaphore.notify();
  _ap->writeThread->join();
  _ap->readThread->join();
  delete _ap->writeThread; _ap->writeThread = nullptr;
  delete _ap->readThread; _ap->readThread = nullptr;
  delete _ap;
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::pushData(const void *ptr, size_t length, Opcode op)
{
  if (!ptr || !length) return;
  {
    HEADSOCKET_LOCK(_ap->writeBlocks);
    _ap->writeBlocks->blockBegin(op);
    _ap->writeBlocks->writeData(ptr, length);
    _ap->writeBlocks->blockEnd();
  }
  _ap->writeSemaphore.notify();
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::pushData(const char *text) { pushData(text, text ? strlen(text) : 0, Opcode::Text); }

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::peekData(Opcode *op) const
{
  HEADSOCKET_LOCK(_ap->readBlocks);
  return _ap->readBlocks->peekData(op);
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::popData(void *ptr, size_t length)
{
  if (!ptr) return InvalidOperation;
  if (!length) return 0;

  HEADSOCKET_LOCK(_ap->readBlocks);
  return _ap->readBlocks->readData(ptr, length);
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::initAsyncThreads()
{
  _ap->writeThread = new std::thread(std::bind(&AsyncTcpClient::writeThread, this));
  _ap->readThread = new std::thread(std::bind(&AsyncTcpClient::readThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::writeThread()
{
  setThreadName("AsyncTcpClient::writeThread");
  std::vector<uint8_t> buffer(1024 * 1024);
  while (_p->isConnected)
  {
    size_t written = 0;
    {
      HEADSOCKET_LOCK(_ap->writeSemaphore);
      if (!_p->isConnected) break;
      written = asyncWriteHandler(buffer.data(), buffer.size());
    }
    if (written == InvalidOperation) break;
    if (!written) buffer.resize(buffer.size() * 2);
    else
    {
      const char *cursor = reinterpret_cast<const char *>(buffer.data());
      while (written)
      {
        int result = send(_p->clientSocket, cursor, written, 0);
        if (!result || result == SOCKET_ERROR) break;
        cursor += result;
        written -= static_cast<size_t>(result);
      }
    }
  }
  killThreads();
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::asyncWriteHandler(uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_ap->writeBlocks);
  // TODO
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_ap->readBlocks);
  // TODO
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
bool AsyncTcpClient::asyncReceivedData(const DataBlock &db, uint8_t *ptr, size_t length) { return false; }

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::readThread()
{
  setThreadName("AsyncTcpClient::readThread");
  std::vector<uint8_t> buffer(1024 * 1024);
  size_t bufferBytes = 0, consumed = 0;
  while (_p->isConnected)
  {
    while (true)
    {
      int result = static_cast<size_t>(bufferBytes);
      if (!result || !consumed)
      {
        result = recv(_p->clientSocket, reinterpret_cast<char *>(buffer.data() + bufferBytes), buffer.size() - bufferBytes, 0);
        if (!result || result == SOCKET_ERROR) { consumed = InvalidOperation; break; }
        bufferBytes += static_cast<size_t>(result);
      }
      consumed = asyncReadHandler(buffer.data(), bufferBytes);
      if (!consumed) { if (bufferBytes == buffer.size()) buffer.resize(buffer.size() * 2); }
      else break;
    }
    if (consumed == InvalidOperation) break;
    bufferBytes -= consumed;
    if (bufferBytes) memcpy(buffer.data(), buffer.data() + consumed, bufferBytes);
  }
  killThreads();
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::killThreads()
{
  if (std::this_thread::get_id() == _ap->readThread->get_id())
    _ap->writeSemaphore.notify();

  disconnect();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(const char *address, int port) : Base(address, port)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(BaseTcpServer *server, ConnectionParams *params): Base(server, params)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::~WebSocketClient()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncWriteHandler(uint8_t *ptr, size_t length)
{
  uint8_t *cursor = ptr;
  HEADSOCKET_LOCK(_ap->writeBlocks);
  while (length >= 16)
  {
    Opcode op;
    size_t toWrite = _ap->writeBlocks->peekData(&op);
    if (!toWrite) break;
    
    size_t toConsume = (length - 15) > FrameSizeLimit ? FrameSizeLimit : (length - 15);
    toConsume = toConsume > toWrite ? toWrite : toConsume;
    
    FrameHeader header;
    header.fin = (toWrite - toConsume) == 0;
    header.opcode = op;
    header.masked = false;
    header.payloadLength = toConsume;

    size_t headerSize = writeFrameHeader(cursor, length, header);
    cursor += headerSize; length -= headerSize;
    _ap->writeBlocks->readData(cursor, toConsume);
    cursor += toConsume; length -= toConsume;

    if (header.fin) _ap->writeSemaphore.consume();
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  uint8_t *cursor = ptr;
  HEADSOCKET_LOCK(_ap->readBlocks);
  if (!_payloadSize)
  {
    Opcode prevOpcode = _currentHeader.opcode;
    size_t headerSize = parseFrameHeader(cursor, length, _currentHeader);
    if (!headerSize) return 0; else if (headerSize == InvalidOperation) return InvalidOperation;
    _payloadSize = _currentHeader.payloadLength;
    cursor += headerSize; length -= headerSize;
    if (_currentHeader.opcode != Opcode::Continuation) _ap->readBlocks->blockBegin(_currentHeader.opcode);
    else _currentHeader.opcode = prevOpcode;
  }

  if (_payloadSize)
  {
    size_t toConsume = length >= _payloadSize ? _payloadSize : length;
    if (toConsume)
    {
      _ap->readBlocks->writeData(cursor, toConsume);
      _payloadSize -= toConsume;
      cursor += toConsume; length -= toConsume;
    }
  }

  if (!_payloadSize)
  {
    if (_currentHeader.masked)
    {
      DataBlock &db = _ap->readBlocks->blocks.back();
      size_t len = _currentHeader.payloadLength;
      Encoding::xor32(_currentHeader.maskingKey, _ap->readBlocks->buffer.data() + _ap->readBlocks->buffer.size() - len, len);
    }

    if (_currentHeader.fin)
    {
      DataBlock &db = _ap->readBlocks->blockEnd();
      switch (_currentHeader.opcode)
      {
        case Opcode::Ping: pushData(_ap->readBlocks->buffer.data() + db.offset, db.length, Opcode::Pong); break;
        case Opcode::Text: _ap->readBlocks->buffer.push_back(0); ++db.length; break;
        case Opcode::ConnectionClose: killThreads(); break;
      }

      if (_currentHeader.opcode == Opcode::Text || _currentHeader.opcode == Opcode::Binary)
        if (asyncReceivedData(db, _ap->readBlocks->buffer.data() + db.offset, db.length)) _ap->readBlocks->blockRemove();
    }
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
#define HAVE_ENOUGH_BYTES(num) if (length < num) return 0; else length -= num;
size_t WebSocketClient::parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header)
{
  uint8_t *cursor = ptr;
  HAVE_ENOUGH_BYTES(2);
  header.fin = ((*cursor) & 0x80) != 0;
  header.opcode = static_cast<Opcode>((*cursor++) & 0x0F);

  header.masked = ((*cursor) & 0x80) != 0;
  uint8_t byte = (*cursor++) & 0x7F;
  if (byte < 126) header.payloadLength = byte;
  else if (byte == 126)
  {
    HAVE_ENOUGH_BYTES(2);
    header.payloadLength = Endian::swap16(*(reinterpret_cast<uint16_t *>(cursor)));
    cursor += 2;
  }
  else if (byte == 127)
  {
    HAVE_ENOUGH_BYTES(8);
    uint64_t length64 = Endian::swap64(*(reinterpret_cast<uint64_t *>(cursor))) & 0x7FFFFFFFFFFFFFFFULL;
    header.payloadLength = static_cast<size_t>(length64);
    cursor += 8;
  }

  if (header.masked)
  {
    HAVE_ENOUGH_BYTES(4);
    header.maskingKey = *(reinterpret_cast<uint32_t *>(cursor));
    cursor += 4;
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::writeFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header)
{
  uint8_t *cursor = ptr;
  HAVE_ENOUGH_BYTES(2);
  *cursor = header.fin ? 0x80 : 0x00;
  *cursor++ |= static_cast<uint8_t>(header.opcode);

  *cursor = header.masked ? 0x80 : 0x00;
  if (header.payloadLength < 126)
  {
    *cursor++ |= static_cast<uint8_t>(header.payloadLength);
  }
  else if (header.payloadLength < 65536)
  {
    HAVE_ENOUGH_BYTES(2);
    *cursor++ |= 126;
    *reinterpret_cast<uint16_t *>(cursor) = Endian::swap16(static_cast<uint16_t>(header.payloadLength));
    cursor += 2;
  }
  else
  {
    HAVE_ENOUGH_BYTES(8);
    *cursor++ |= 127;
    *reinterpret_cast<uint64_t *>(cursor) = Endian::swap64(static_cast<uint64_t>(header.payloadLength));
    cursor += 8;
  }

  if (header.masked)
  {
    HAVE_ENOUGH_BYTES(4);
    *reinterpret_cast<uint32_t *>(cursor) = header.maskingKey;
    cursor += 4;
  }

  return cursor - ptr;
}
#undef HAVE_ENOUGH_BYTES

}
#endif
#endif
