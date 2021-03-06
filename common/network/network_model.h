#ifndef NETWORK_MODEL_H
#define NETWORK_MODEL_H

class NetPacket;
class Network;

#include <vector>
#include <queue>
#include <string>
using std::queue;
using std::vector;
using std::string;
using std::pair;

#include "lock.h"
#include "config.h"
#include "packet_type.h"
#include "fixed_types.h"

#define CORE_ID(x)         ((core_id_t) {x, MAIN_CORE_TYPE})
#define TILE_ID(x)         (x.tile_id)

// -- Network Models -- //

// To implement a new network model, you must implement this routing
// object. To route, take a packet and compute the next hop(s) and the
// time stamp for when that packet will be forwarded.
//   This lets one implement "magic" networks, analytical models,
// realistic hop-by-hop modeling, as well as broadcast models, such as
// a bus or ATAC.  Each static network has its own model object. This
// lets the user network be modeled accurately, while the MCP is a
// stupid magic network.
//   A packet will be dropped if no hops are filled in the nextHops
// vector.
class NetworkModel
{
public:
   NetworkModel(Network *network, SInt32 network_id);
   virtual ~NetworkModel() { }

   class Hop
   {
   public:
      Hop(const NetPacket& pkt, tile_id_t next_tile_id, SInt32 next_node_type,
          UInt64 zero_load_delay = 0, UInt64 contention_delay = 0);
      ~Hop();

      // Next destinations of a packet
      tile_id_t _next_tile_id;
      // Next Node Type (can mean router type)
      SInt32 _next_node_type;
      // This field fills in the 'time' field in NetPacket
      UInt64 _time;
      // This field fill in the 'zero_load_delay' field in NetPacket
      UInt64 _zero_load_delay;
      // This field fills in the 'contention_delay' field in NetPacket
      UInt64 _contention_delay;
   };

   volatile float getFrequency() { return _frequency; }
   bool hasBroadcastCapability() { return _has_broadcast_capability; }

   bool isPacketReadyToBeReceived(const NetPacket& pkt);
   void __routePacket(const NetPacket &pkt, queue<Hop> &next_hops);
   void __processReceivedPacket(NetPacket &pkt);

   virtual void outputSummary(std::ostream &out) = 0;

   void enable() { _enabled = true; }
   void disable() { _enabled = false; }

   static NetworkModel *createModel(Network* network, SInt32 network_id, UInt32 model_type);
   static UInt32 parseNetworkType(string str);

   static bool isTileCountPermissible(UInt32 network_type, SInt32 tile_count);
   static pair<bool, vector<tile_id_t> > computeMemoryControllerPositions(UInt32 network_type, SInt32 num_memory_controllers, SInt32 total_tiles);
   static pair<bool, vector<Config::TileList> > computeProcessToTileMapping(UInt32 network_type);

   // SEND_TILE, RECEIVE_TILE
   static const SInt32 SEND_TILE = 0;
   static const SInt32 RECEIVE_TILE = 1;

   // Is Model Enabled
   bool isModelEnabled(const NetPacket& pkt);
   // Get Modeled Length (in bits)
   UInt32 getModeledLength(const NetPacket& pkt);
   // Compute Number of Flits
   SInt32 computeNumFlits(UInt32 pkt_length);

   // Tracing Network Injection/Ejection Rate
   void popCurrentUtilizationStatistics(UInt64& total_flits_sent, UInt64& total_flits_broadcasted, UInt64& total_flits_received);

protected:
   class NextDest
   {
   public:
      NextDest()
         : _tile_id(INVALID_TILE_ID), _output_port(-1), _node_type(-1) {}
      NextDest(tile_id_t tile_id, SInt32 output_port, SInt32 node_type)
         : _tile_id(tile_id), _output_port(output_port), _node_type(node_type) {}
      ~NextDest() {}

      tile_id_t _tile_id;
      SInt32 _output_port;
      SInt32 _node_type;
   };

   // Frequency
   volatile float _frequency;
   // Flit Width
   SInt32 _flit_width;
   // Has Broadcast Capability
   bool _has_broadcast_capability;
   // Tile ID
   tile_id_t _tile_id;
   // Tile Width
   volatile double _tile_width;

   Network *getNetwork() { return _network; }
   SInt32 getNetworkId() { return _network_id; }

   // Is Application Tile?
   bool isApplicationTile(tile_id_t tile_id);
   // Is System Tile - Thread Spawner or MCP
   bool isSystemTile(tile_id_t tile_id);

private:
   Network *_network;
   
   SInt32 _network_id;
   string _network_name;
   bool _enabled;

   // Lock
   Lock _lock;

   // Event Counters
   UInt64 _total_packets_sent;
   UInt64 _total_flits_sent;
   UInt64 _total_bits_sent;

   UInt64 _total_packets_broadcasted;
   UInt64 _total_flits_broadcasted;
   UInt64 _total_bits_broadcasted;

   UInt64 _total_packets_received;
   UInt64 _total_flits_received;
   UInt64 _total_bits_received;

   UInt64 _total_packet_latency;
   UInt64 _total_contention_delay;

   // For getting a trace of network injection/ejection rate
   UInt64 _total_flits_sent_in_current_interval;
   UInt64 _total_flits_broadcasted_in_current_interval;
   UInt64 _total_flits_received_in_current_interval;

   virtual void routePacket(const NetPacket &pkt, queue<Hop> &next_hops) = 0;
   virtual void processReceivedPacket(NetPacket &pkt);
  
   // Process Corner Cases
   bool processCornerCases(const NetPacket &pkt, queue<Hop> &next_hops);

   // Update Send & Receive Counters
   void updateSendCounters(const NetPacket& packet);
   void updateReceiveCounters(const NetPacket& packet);

   // Initialize Event Counters
   void initializeEventCounters();
   // Trace of Injection/Ejection Rate
   void initializeCurrentUtilizationStatistics();
};

#endif // NETWORK_MODEL_H
