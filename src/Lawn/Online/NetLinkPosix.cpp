// SPDX-License-Identifier: CC0-1.0
// POSIX transport for the public NetLink interface from pinned PVZ Online Build 44.
#include "NetLink.h"
#include "NetOutbound.h"
#include "LegacyObserverQueue.h"
#include "NetworkLog.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#ifdef __ANDROID__
#include <jni.h>
#endif

static std::atomic<bool> privateAppBackground{false};
#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_io_github_wszqkzqk_pvzportable_PvZPortableActivity_nativePrivateBackground(JNIEnv*,jclass,jboolean value) {
    privateAppBackground.store(value==JNI_TRUE);
}
#endif

namespace
{
using State = NetLink::State;
using namespace std::chrono_literals;
#if defined(__APPLE__)
constexpr int kSendFlags = 0; // macOS uses SO_NOSIGPIPE instead.
#else
constexpr int kSendFlags = MSG_NOSIGNAL;
#endif
struct Worker
{
    NetOutbound outbound;NetOutbound::Bytes outgoing;size_t outgoingAt=0;bool outgoingDisplay=false;
    std::chrono::steady_clock::time_point stalledSince{};
    bool FlushWrites(){for(int quota=0;quota<8;++quota){
        if(outgoing.empty()){auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();if(!outbound.Pop(outgoing,outgoingDisplay,now))return true;outgoingAt=0;stalledSince=std::chrono::steady_clock::now();}
        int n=send(socket,outgoing.data()+outgoingAt,outgoing.size()-outgoingAt,kSendFlags);
        if(n>0){outgoingAt+=n;stalledSince=std::chrono::steady_clock::now();if(outgoingAt==outgoing.size()){outbound.Completed(outgoingDisplay);bool optional=outgoingDisplay;outgoing.clear();/* Recheck critical before each of at most eight fragments per read slice. */}continue;}
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)){if(std::chrono::steady_clock::now()-stalledSince<5s)return true;}
        Fail("Connection lost while sending");return false;
    }return true;}
    std::atomic<State> state{State::IDLE};
    std::atomic<bool> stop{false};
    std::atomic<bool> backgroundSensitive{false};
    bool Suspended() {
        if(!backgroundSensitive.load()||!privateAppBackground.load())return false;
        if(socket>=0)shutdown(socket,SHUT_RDWR);
        Fail("The local player paused. Reconnecting when the game resumes.");
        return true;
    }
    std::atomic<int> socket{-1}, listener{-1}, attempts{0};
    std::thread thread;
    std::mutex queueMutex, errorMutex, doneMutex;
    std::condition_variable doneCv;
    bool done = false;
    std::deque<NetLink::Packet> queue;
    std::string host, error;
    uint16_t port = 0;

    ~Worker()
    {
        if (socket >= 0) ::close(socket);
        if (listener >= 0) ::close(listener);
    }
    void Fail(const std::string& message)
    {
        if (stop) return;
        { std::lock_guard lock(errorMutex); error = message; }
        state = State::FAILED;
        NetworkLog("[net] link failed: %s", message.c_str());
    }
    bool Wait(int fd, short events)
    {
        pollfd descriptor{fd, events, 0};
        const int result = poll(&descriptor, 1, 10);
        if (result < 0 && errno != EINTR) Fail(std::strerror(errno));
        return result > 0 && (descriptor.revents & (events | POLLERR | POLLHUP | POLLNVAL));
    }
    static void Configure(int fd)
    {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        const int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#if defined(__APPLE__)
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
    }
    void Publish(int fd)
    {
        Configure(fd);
        socket = fd;
        state = State::CONNECTED;
    }
    void ConnectAttempt()
    {
        ++attempts;
        addrinfo hints{}, *addresses = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        const std::string service = std::to_string(port);
        if (getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses) == 0)
        {
            for (auto* address = addresses; address && !stop; address = address->ai_next)
            {
                int fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (fd < 0) continue;
                Configure(fd);
                bool connected = ::connect(fd, address->ai_addr, address->ai_addrlen) == 0;
                if (!connected && errno == EINPROGRESS)
                {
                    const auto deadline = std::chrono::steady_clock::now() + 5s;
                    while (!stop && std::chrono::steady_clock::now() < deadline)
                    {
                        if (!Wait(fd, POLLOUT)) continue;
                        int code = 0;
                        socklen_t length = sizeof(code);
                        connected = getsockopt(fd, SOL_SOCKET, SO_ERROR, &code, &length) == 0 && code == 0;
                        break;
                    }
                }
                if (connected && !stop) { Publish(fd); break; }
                ::close(fd);
            }
            freeaddrinfo(addresses);
        }
        if (state == State::CONNECTING && !stop)
        {
            std::unique_lock lock(doneMutex);
            doneCv.wait_for(lock, 1s, [&] { return stop.load(); });
        }
    }
    bool ReceiveExact(uint8_t* bytes, int size)
    {
        int received = 0;
        while (received < size && !stop && state == State::CONNECTED)
        {
            if(Suspended())return false;
            if(!FlushWrites())return false;
            if (!Wait(socket, POLLIN)) continue;
            int count = recv(socket, bytes + received, size - received, 0);
            if (count > 0) { received += count; continue; }
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
            Fail(count == 0 ? "The other player closed the connection." : std::strerror(errno));
            return false;
        }
        return received == size;
    }
    void ReceivePacket()
    {
        NetLink::Packet packet{};
        if (!ReceiveExact(packet.mData, NetProto::HEADER_SIZE)) return;
        int payload = packet.mData[2] | (packet.mData[3] << 8);
        if (payload > NetProto::MAX_WIRE_PAYLOAD) { Fail("A player sent a malformed packet."); return; }
        if (payload && !ReceiveExact(packet.mData + NetProto::HEADER_SIZE, payload)) return;
        packet.mSize = NetProto::HEADER_SIZE + payload;
        std::lock_guard lock(queueMutex);
        if (LegacyObserverQueue::IsSnapshot(packet.mData, packet.mSize) && queue.size() >= LegacyObserverQueue::ReceiveSnapshotBudget) return;
        if (queue.size() >= 256) { Fail("Too many unread packets - the connection is stalled."); return; }
        queue.push_back(packet);
    }
    void Run()
    {
        while (!stop)
        {
            if(Suspended())break;
            if (state == State::LISTENING)
            {
                if (Wait(listener, POLLIN))
                {
                    int fd = accept(listener, nullptr, nullptr);
                    if (fd >= 0) { if (stop) ::close(fd); else Publish(fd); }
                }
            }
            else if (state == State::CONNECTING) ConnectAttempt();
            else if (state == State::CONNECTED) ReceivePacket();
            else break;
        }
        { std::lock_guard lock(doneMutex); done = true; }
        doneCv.notify_all();
    }
    bool Start(const std::shared_ptr<Worker>& owner)
    {
        try { thread = std::thread([owner] { owner->Run(); }); return true; }
        catch (...) { Fail("Cannot start the network thread."); return false; }
    }
};
}

struct NetLink::Impl { std::shared_ptr<Worker> worker = std::make_shared<Worker>(); };
NetLink::NetLink() : mImpl(new Impl) {}
void NetLink::SetBackgroundSensitive(bool value){mImpl->worker->backgroundSensitive=value;}
NetLink::~NetLink() { Close(); delete mImpl; }

void NetLink::Close()
{
    if (!mImpl) return;
    auto worker = mImpl->worker;
    auto drain=std::chrono::steady_clock::now();
    while(worker->state==State::CONNECTED&&worker->outbound.HasCritical()&&std::chrono::steady_clock::now()-drain<200ms)std::this_thread::sleep_for(1ms);
    worker->stop = true;
    if (worker->socket >= 0) shutdown(worker->socket, SHUT_RDWR);
    if (worker->listener >= 0) shutdown(worker->listener, SHUT_RDWR);
    worker->doneCv.notify_all();
    if (worker->thread.joinable())
    {
        std::unique_lock lock(worker->doneMutex);
        bool finished = worker->doneCv.wait_for(lock, 2s, [&] { return worker->done; });
        lock.unlock();
        if (finished) worker->thread.join();
        else worker->thread.detach(); // A stalled resolver retains its own owner until it exits.
    }
    mImpl->worker = std::make_shared<Worker>();
}

bool NetLink::Listen(uint16_t port)
{
    Close();
    auto worker = mImpl->worker;
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) { worker->Fail(std::strerror(errno)); return false; }
    worker->listener = fd;
    const int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    Worker::Configure(fd);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || listen(fd, 1) < 0)
    { worker->Fail(std::strerror(errno)); return false; }
    worker->state = State::LISTENING;
    return worker->Start(worker);
}
bool NetLink::Connect(const char* host, uint16_t port)
{
    Close();
    auto worker = mImpl->worker;
    if (!host || !*host || std::strlen(host) >= 64) { worker->Fail("The server address is invalid."); return false; }
    worker->host = host;
    worker->port = port;
    worker->state = State::CONNECTING;
    return worker->Start(worker);
}
NetLink::State NetLink::GetState() const { return mImpl->worker->state; }
bool NetLink::IsConnected() const { return GetState() == State::CONNECTED; }
int NetLink::GetConnectAttempts() const { return std::max(1, mImpl->worker->attempts.load()); }
bool NetLink::Send(const void*data,int size){auto w=mImpl->worker;if(w->state!=State::CONNECTED)return false;if(w->outbound.Critical(data,size))return true;w->Fail("Too many pending gameplay messages.");return false;}
bool NetLink::OfferObserver(uint8_t viewer,std::vector<std::vector<uint8_t>> packets){auto w=mImpl->worker;return w->state==State::CONNECTED&&w->outbound.Display(viewer,std::move(packets));}
bool NetLink::ObserverWritable()const{auto w=mImpl->worker;return w->state==State::CONNECTED&&!w->outbound.HasCritical()&&w->outbound.DisplayBytes()==0;}
void NetLink::ClearObserverOutput(uint8_t viewer){mImpl->worker->outbound.ClearDisplay(viewer);}
bool NetLink::Poll(Packet& packet)
{
    auto worker = mImpl->worker;
    std::lock_guard lock(worker->queueMutex);
    if (worker->queue.empty()) return false;
    packet = worker->queue.front(); worker->queue.pop_front(); return true;
}
void NetLink::ClearQueue()
{
    auto worker = mImpl->worker;
    std::lock_guard lock(worker->queueMutex); worker->queue.clear();
}
const char* NetLink::GetLastError() const
{
    thread_local std::string text;
    auto worker = mImpl->worker;
    std::lock_guard lock(worker->errorMutex); text = worker->error; return text.c_str();
}
std::string NetLink::GetLocalIPv4Text()
{
    ifaddrs* addresses = nullptr;
    std::string fallback = "127.0.0.1", preferred;
    if (getifaddrs(&addresses) == 0)
    {
        for (auto* item = addresses; item; item = item->ifa_next)
        {
            if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET) continue;
            auto* address = reinterpret_cast<sockaddr_in*>(item->ifa_addr);
            uint32_t host = ntohl(address->sin_addr.s_addr);
            if (!host || host >> 24 == 127) continue;
            char text[INET_ADDRSTRLEN];
            if (!inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text))) continue;
            if (host >> 24 == 192 || host >> 24 == 10) { preferred = text; break; }
            fallback = text;
        }
        freeifaddrs(addresses);
    }
    return preferred.empty() ? fallback : preferred;
}
