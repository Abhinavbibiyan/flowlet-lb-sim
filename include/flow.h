#pragma once
#include <cstdint>
#include <string>

// 5-tuple identifying a flow
struct FlowKey {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;

    bool operator==(const FlowKey& o) const {
        return src_ip == o.src_ip && dst_ip == o.dst_ip &&
               src_port == o.src_port && dst_port == o.dst_port &&
               proto == o.proto;
    }
};

struct FlowKeyHash {
    size_t operator()(const FlowKey& k) const {
        // FNV-1a inspired mix
        size_t h = 2166136261ULL;
        auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ULL; };
        mix(k.src_ip);
        mix(k.dst_ip);
        mix(((uint32_t)k.src_port << 16) | k.dst_port);
        mix(k.proto);
        return h;
    }
};

// A single network flow
struct Flow {
    int      id;
    FlowKey  key;
    int      src_leaf;     // source ToR switch
    int      dst_leaf;     // destination ToR switch
    uint64_t size_bytes;   // total flow size
    double   start_time;   // seconds
    double   end_time;     // filled when complete
    double   first_packet; // time of first packet sent
    double   last_packet;  // time of last packet sent

    // Routing state
    int      current_spine; // which spine is being used

    uint64_t bytes_sent;
    bool     completed;

    // FCT metrics
    double fct() const {
        if (completed && end_time > start_time)
            return end_time - start_time;
        return -1.0;
    }

    double ideal_fct(double link_gbps) const {
        // serialisation + 2 hops (leaf->spine->leaf) RTT ~= 2*2us + transfer
        double prop_rtt = 4e-6; // 4 microseconds
        double xfer = (double)size_bytes * 8.0 / (link_gbps * 1e9);
        return prop_rtt + xfer;
    }

    Flow() : id(0), src_leaf(0), dst_leaf(0), size_bytes(0),
             start_time(0), end_time(0), first_packet(0), last_packet(0),
             current_spine(-1), bytes_sent(0), completed(false) {}
};

// Packet event in the simulation
struct Packet {
    int      flow_id;
    FlowKey  key;
    uint32_t size_bytes;
    double   timestamp;    // injection time
    int      src_leaf;
    int      dst_leaf;
    int      spine;        // assigned spine path
};
