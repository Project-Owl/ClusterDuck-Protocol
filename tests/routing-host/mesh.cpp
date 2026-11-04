#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include "CdpPacket.h"

class FakeRadio {
public:
    std::deque<std::vector<uint8_t>> inbox;
    inline static std::function<void(FakeRadio*, const std::vector<uint8_t>&)> deliver;
    uint32_t done = 0;
    static void onInterrupt() {}
    inline static const LoRaConfigParams defaultRadioParams{915, 22, 125, 7, 0, onInterrupt};
    int setupRadio(const LoRaConfigParams&) { return 0; }
    void serviceInterruptFlags() {}
    bool getReceiveFlag() { return !inbox.empty(); }
    std::optional<std::vector<uint8_t>> readReceivedData() {
        if (inbox.empty()) return std::nullopt;
        auto data = inbox.front(); inbox.pop_front(); return data;
    }
    int sendData(std::vector<uint8_t> data) { ++done; deliver(this, data); return 0; }
    int startReceive() { return 0; }
    uint32_t getTxDoneCount() const { return done; }
    uint32_t getTxTimeoutCount() const { return 0; }
    float getRSSI() { return -50; }
    float getSNR() { return 12; }
};

#include "Ducks/MamaDuck.h"
#include "Ducks/PapaDuck.h"

Duid id(const std::string& name) { assert(name.size() == 8); Duid value{}; std::copy(name.begin(), name.end(), value.begin()); return value; }
using Mama = MamaDuck<DuckWifiNone, FakeRadio>;
using Papa = PapaDuck<DuckWifiNone, FakeRadio>;
template<class Base> struct Node : Base {
    explicit Node(std::string name) : Base(name) { this->onReceiveDuckData(receive); }
    FakeRadio& radio() { return this->duckRadio; }
    DuckRouter& routes() { return this->router; }
    inline static std::vector<CdpPacket> received;
    static void receive(CdpPacket packet) { received.push_back(packet); }
};

struct Mesh {
    Node<Mama> a{"NODE0001"}, b{"NODE0002"}, c{"NODE0003"};
    Node<Papa> p{"GATE0001"};
    std::vector<FakeRadio*> radios{&a.radio(), &b.radio(), &c.radio(), &p.radio()};
    bool links[4][4]{};
    struct Sent { size_t sender; CdpPacket packet; };
    std::vector<Sent> sent;
    Mesh(bool complete) {
        hostMillis = 0;
        Node<Mama>::received.clear(); Node<Papa>::received.clear();
        for (size_t i = 0; i < 4; ++i) for (size_t j = 0; j < 4; ++j)
            links[i][j] = i != j && (complete || i + 1 == j || j + 1 == i);
        FakeRadio::deliver = [this](FakeRadio* from, const std::vector<uint8_t>& bytes) {
            const size_t sender = std::find(radios.begin(), radios.end(), from) - radios.begin();
            CdpPacket packet(bytes);
            sent.push_back({sender, packet});
            for (size_t j = 0; j < radios.size(); ++j) if (links[sender][j]) radios[j]->inbox.push_back(bytes);
        };
        assert(a.setupWithDefaults() == 0); assert(b.setupWithDefaults() == 0);
        assert(c.setupWithDefaults() == 0); assert(p.setupWithDefaults() == 0);
    }
    void until(uint32_t end) {
        for (; hostMillis < end; hostMillis += 100) { a.run(); b.run(); c.run(); p.run(); }
    }
    size_t count(uint8_t topic, size_t from = 0) const {
        return std::count_if(sent.begin() + from, sent.end(), [topic](const auto& item) { return item.packet.topic == topic; });
    }
};

JsonDocument parse(const std::string& text) { JsonDocument doc; assert(!deserializeJson(doc, text)); return doc; }
void routerChecks() {
    hostMillis = 100;
    const SignalScore strong{0.95f, 0.75f, 0.85f}, weak{0.5f, 0.5f, 0.5f};
    DuckRouter router;
    router.insertIntoRoutingTable(id("NODE0001"), id("NODE0001"), strong);
    router.insertIntoRoutingTable(id("NODE0001"), id("NODE0002"), weak);
    auto doc = parse(*router.getEntriesFor(PAPADUCK_DUID, id("SELF0001")));
    assert(doc["n"].size() == 1); // indirect update did not erase direct measurement
    router.insertIntoRoutingTable(id("TARGET01"), id("NODE0001"), strong);
    hostMillis += CDPCFG_ROUTE_TTL_MS - 1;
    router.insertIntoRoutingTable(id("TARGET01"), id("NODE0002"), weak);
    ++hostMillis;
    assert(router.getBestNextHop(id("TARGET01")) == id("NODE0002"));
    assert(router.directNeighborCount() == 0);
    for (size_t count : {0, 1, 2, 3, 6, 7, 40}) {
        DuckRouter peers;
        for (size_t i = 0; i < count; ++i) {
            char peer[9]; snprintf(peer, sizeof(peer), "PEER%04u", unsigned(i));
            peers.insertIntoRoutingTable(id(peer), id(peer), strong);
            peers.insertIntoRoutingTable(id(peer), id(peer), strong); // same millisecond, no duplicate
        }
        const auto payload = *peers.getEntriesFor(PAPADUCK_DUID, id("SELF0001"));
        assert(payload.size() <= MAX_DATA_LENGTH);
        auto detail = parse(payload);
        if (count == 3) {
            std::cout << "SIGNAL_FIXTURE=" << payload << '\n';
            CdpPacket fixture(PAPADUCK_DUID, topics::sig, duckutils::stringToByteVector(payload), id("SELF0001"), DuckType::MAMA);
            fixture.muid = {'F','I','X','1'};
            assert(fixture.prepareForSending() == 0);
            auto bytes = fixture.asBytes();
            std::cout << "PACKET_FIXTURE=" << duckutils::convertToHex(bytes.data(), bytes.size()) << '\n';
        }
        assert(detail["o"].as<size_t>() == std::min(count, size_t(CDPCFG_MAX_DIRECT_NEIGHBORS)));
        assert(detail["i"].as<size_t>() == detail["n"].size());
        if (count <= 3) assert(detail["n"].size() == count);
        assert(detail["tr"].as<bool>() == (detail["o"].as<size_t>() > detail["n"].size()));
        std::cout << "neighbors=" << count << " included=" << detail["n"].size() << " bytes=" << payload.size() << '\n';
    }
    // Forward-only culling across >= four destinations and uint32 rollover.
    hostMillis = UINT32_MAX - 100;
    DuckRouter rollover;
    for (int i = 0; i < 5; ++i) {
        char peer[9]; snprintf(peer, sizeof(peer), "PEER%04u", unsigned(i));
        rollover.insertIntoRoutingTable(id(peer), id(peer), strong);
    }
    hostMillis = 200;
    assert(rollover.getBestNextHop(id("PEER0001")));
    hostMillis += CDPCFG_ROUTE_TTL_MS;
    rollover.cullRoutingTable();
    assert(rollover.directNeighborCount() == 0);
    // Length/type checks, duplicate path nodes, malformed and long payloads.
    for (std::string text : {"{}", "{\"origin\":\"bad\",\"destination\":\"NODE0001\",\"path\":[]}",
        "{\"origin\":\"NODE0001\",\"destination\":\"NODE0002\",\"path\":[\"NODE0001\",\"NODE0001\"]}"})
        assert(!RouteJSON::fromPacketData(duckutils::stringToByteVector(text)));
    RouteJSON broadcast(BROADCAST_DUID, id("NODE0001")); broadcast.addToPath(id("NODE0001"));
    assert(RouteJSON::fromPacketData(duckutils::stringToByteVector(broadcast.asString()))->getDestination() == BROADCAST_DUID);
    SizedQueue queue(1);
    CdpPacket packet(PAPADUCK_DUID, topics::health, {1}, id("NODE0001"), DuckType::MAMA);
    packet.hopCount = 0;
    packet.data.assign(228, 1); assert(packet.prepareForSending() == 0);
    packet.data.assign(229, 1); assert(packet.prepareForSending() == DUCKPACKET_ERR_SIZE_INVALID);
    packet.data.assign(1, 1);
    assert(queue.enqueue(packet)); assert(!queue.enqueue(packet)); assert(queue.dropped == 1);
    packet.data.assign(256, 1); assert(packet.prepareForSending() == DUCKPACKET_ERR_SIZE_INVALID);
    packet.data.assign(1, 1); packet.hopCount = MAX_HOPS + 1;
    assert(packet.prepareForSending() == DUCKPACKET_ERR_MAX_HOPS);
}

int main() {
    routerChecks();
    {
        Mesh mesh(true);
        mesh.until(120000);
        size_t start = mesh.sent.size();
        mesh.until(720000);
        const auto requests = mesh.count(reservedTopic::rreq, start);
        const auto responses = mesh.count(reservedTopic::rrep, start);
        std::cout << "complete mesh / 10 quiet minutes: RREQ=" << requests << " RREP=" << responses << '\n';
        assert(requests <= 6 && responses <= 18);
        assert(mesh.a.routes().getBestNextHop(PAPADUCK_DUID) == id("GATE0001"));
        assert(mesh.a.routes().directNeighborCount() == 3);
        for (uint8_t page = 0; page < 4; ++page) {
            const std::string payload = mesh.a.routingDiagnostics(page);
            assert(!payload.empty() && payload.size() <= MAX_DATA_LENGTH);
            auto doc = parse(payload); assert(doc["p"] == page);
            // Prove the page budget even at maximum 32-bit counter values.
            for (auto pair : doc.as<JsonObject>()) {
                std::string key = pair.key().c_str();
                if (key == "v" || key == "p") continue;
                if (pair.value().is<JsonArray>()) for (auto value : pair.value().as<JsonArray>()) value.set(UINT32_MAX);
                else if (pair.value().is<uint32_t>()) pair.value().set(UINT32_MAX);
            }
            assert(measureJson(doc) <= MAX_DATA_LENGTH);
        }
        CdpPacket duplicate(PAPADUCK_DUID, topics::sensor, {1}, id("NODE0001"), DuckType::MAMA);
        duplicate.muid = {'D','U','P','1'}; assert(duplicate.prepareForSending() == 0);
        const auto receiptCount = Node<Papa>::received.size();
        mesh.p.radio().inbox.push_back(duplicate.asBytes());
        mesh.p.radio().inbox.push_back(duplicate.asBytes());
        mesh.p.run(); mesh.p.run();
        assert(Node<Papa>::received.size() == receiptCount + 2); // MQTT duplicate policy preserved
        // A full data queue is an explicit failure, not false success.
        unsigned accepted = 0;
        for (unsigned i = 0; i < 20; ++i)
            if (mesh.a.sendData(topics::sensor, "{\"c\":1}") == 0) ++accepted;
        assert(accepted == CDPCFG_MAX_QUEUE_SIZE);
    }
    {
        Mesh mesh(false);
        mesh.until(120000);
        assert(mesh.a.sendData(topics::sensor, "{\"c\":1}") == 0);
        mesh.until(210000);
        assert(mesh.a.routes().getBestNextHop(PAPADUCK_DUID) == id("NODE0002"));
        assert(mesh.a.routes().directNeighborCount() == 1);
        assert(std::any_of(Node<Papa>::received.begin(), Node<Papa>::received.end(), [](const auto& packet) {
            return packet.topic == topics::sensor && packet.sduid == id("NODE0001");
        }));
        std::map<std::tuple<Duid,Muid>, std::set<int>> replies;
        for (const auto& item : mesh.sent) if (item.packet.topic == reservedTopic::rrep)
            replies[{item.packet.sduid, item.packet.muid}].insert(item.packet.hopCount);
        bool threeHops = false;
        for (const auto& [key, hops] : replies) if (hops == std::set<int>{0,1,2}) threeHops = true;
        assert(threeHops); // a reply traversed all three links without a new ID
        std::cout << "three-link discovery: gateway route converged; reply identity preserved\n";
        // Gateway loss, quiet late arrival and route expiry/recovery.
        mesh.links[2][3] = mesh.links[3][2] = false;
        mesh.until(2400000);
        assert(!mesh.a.routes().getBestNextHop(PAPADUCK_DUID));
        assert(mesh.a.sendData(topics::health, "{\"x\":1}") == 0); // standalone still accepts
        mesh.links[2][3] = mesh.links[3][2] = true;
        mesh.until(3100000);
        assert(mesh.a.routes().getBestNextHop(PAPADUCK_DUID));
        std::cout << "gateway loss/expiry/late return: recovered without a membership handshake\n";
        // A deep sleep reboot loses ordinary RAM, routing and duplicate history.
        mesh.b.~Node<Mama>(); new (&mesh.b) Node<Mama>("NODE0002");
        assert(mesh.b.setupWithDefaults() == 0);
        assert(mesh.b.sendData(topics::health, "{\"wake\":1}") == 0);
        mesh.until(3200000);
        assert(mesh.b.routes().getBestNextHop(PAPADUCK_DUID));
        std::cout << "relay reboot/wake: accepts immediately and rediscovers gateway\n";
    }
    {
        Mesh mesh(true);
        assert(mesh.a.sendData(topics::health, "{\"early\":1}") == 0);
        mesh.a.run();
        auto before = parse(mesh.a.routingDiagnostics(1));
        assert(before["q"][2].as<unsigned>() == 1);
        hostMillis = 2000; mesh.a.run();
        auto after = parse(mesh.a.routingDiagnostics(1));
        assert(after["q"][2].as<unsigned>() >= 1);
        assert(mesh.sent.empty()); // request was not dequeued before pacing allowed TX
        mesh.until(120000);
        assert(std::any_of(Node<Papa>::received.begin(), Node<Papa>::received.end(), [](const auto& packet) {
            return packet.topic == topics::health && packet.sduid == id("NODE0001");
        }));
        std::cout << "startup send and request pacing: accepted packet reaches gateway\n";
    }
    {
        // 5.0 MamaDucks only relay data addressed to 00000000, so data for the
        // PapaDuck must keep that header address. Route JSON uses the text alias.
        Mesh mesh(true);
        mesh.until(120000);
        const size_t start = mesh.sent.size();
        assert(mesh.a.sendData(topics::sensor, "{\"c\":2}") == 0);
        mesh.until(130000);
        bool zeroHeader = false;
        for (size_t i = start; i < mesh.sent.size(); ++i)
            if (mesh.sent[i].packet.topic == topics::sensor && mesh.sent[i].packet.sduid == id("NODE0001"))
                zeroHeader = mesh.sent[i].packet.dduid == Duid{};
        assert(zeroHeader);
        RouteJSON toPapa(PAPADUCK_DUID, id("NODE0001"));
        toPapa.addToPath(id("NODE0001"));
        const std::string text = toPapa.asString();
        assert(text.find("\"destination\":\"11111111\"") != std::string::npos);
        assert(text.find('\0') == std::string::npos);
        assert(RouteJSON::fromPacketData(duckutils::stringToByteVector(text))->getDestination() == PAPADUCK_DUID);
        // Phoenix 2026 firmware addressed the PapaDuck as 11111111 in the header.
        CdpPacket legacy(PAPADUCK_ALIAS_DUID, topics::sensor, {1}, id("NODE0003"), DuckType::MAMA);
        legacy.muid = {'L','E','G','1'}; assert(legacy.prepareForSending() == 0);
        const auto receipts = Node<Papa>::received.size();
        mesh.p.radio().inbox.push_back(legacy.asBytes());
        for (int i = 0; i < 3; ++i) mesh.p.run();
        assert(Node<Papa>::received.size() == receipts + 1);
        assert(Node<Papa>::received.back().dduid == PAPADUCK_DUID);
        std::cout << "5.0 compatibility: header 00000000, route JSON 11111111, legacy header accepted\n";
    }
    std::cout << "PASS\n";
}
