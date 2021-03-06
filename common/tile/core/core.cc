#include "core.h"
#include "main_core.h"
#include "tile.h"
#include "network.h"
#include "syscall_model.h"
#include "sync_client.h"
#include "network_types.h"
#include "memory_manager.h"
#include "pin_memory_manager.h"
#include "clock_skew_minimization_object.h"
#include "core_model.h"
#include "simulator.h"
#include "log.h"

using namespace std;

Core::Core(Tile *tile, core_type_t core_type)
   : m_tile(tile)
   , m_core_id((core_id_t) {tile->getId(), core_type})
   , m_core_state(IDLE)
   , m_pin_memory_manager(NULL)
{
   m_network = m_tile->getNetwork();
   m_shmem_perf_model = m_tile->getShmemPerfModel();
   m_memory_manager = m_tile->getMemoryManager();

   m_core_model = CoreModel::create(this);
   m_sync_client = new SyncClient(this);
   m_syscall_model = new SyscallMdl(tile->getNetwork());
   m_clock_skew_minimization_client =
      ClockSkewMinimizationClient::create(Sim()->getCfg()->getString("clock_skew_minimization/scheme","none"), this);
    
   if (Config::getSingleton()->isSimulatingSharedMemory())
      m_pin_memory_manager = new PinMemoryManager(this);
}

Core::~Core()
{
   LOG_PRINT("Deleting core on tile %d", m_core_id.tile_id);

   if (m_pin_memory_manager)
      delete m_pin_memory_manager;

   if (m_clock_skew_minimization_client)
      delete m_clock_skew_minimization_client;
   delete m_syscall_model;
   delete m_sync_client;
   delete m_core_model;
}

int Core::coreSendW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   core_id_t receiver_core = (core_id_t) {receiver, this->getCoreType()};

   SInt32 sent;
   if (receiver == CAPI_ENDPOINT_ALL)
      sent = m_tile->getNetwork()->netBroadcast(pkt_type, buffer, size);
   else
      sent = m_tile->getNetwork()->netSend(receiver_core, pkt_type, buffer, size);
   
   LOG_ASSERT_ERROR(sent == size, "Bytes Sent(%i), Message Size(%i)", sent, size);

   return sent == size ? 0 : -1;
}

int Core::coreRecvW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   core_id_t sender_core = (core_id_t) {sender, getCoreType()};

   NetPacket packet;
   if (sender == CAPI_ENDPOINT_ANY)
      packet = m_tile->getNetwork()->netRecvType(pkt_type, m_core_id);
   else
      packet = m_tile->getNetwork()->netRecv(sender_core, m_core_id, pkt_type);

   LOG_PRINT("Got packet: from {%i, %i}, to {%i, %i}, type %i, len %i", packet.sender.tile_id, packet.sender.core_type, packet.receiver.tile_id, packet.receiver.core_type, (SInt32)packet.type, packet.length);

   LOG_ASSERT_ERROR((unsigned)size == packet.length, "Tile: User thread requested packet of size: %d, got a packet from %d of size: %d", size, sender, packet.length);

   memcpy(buffer, packet.data, size);

   // De-allocate dynamic memory
   // Is this the best place to de-allocate packet.data ??
   delete [](Byte*)packet.data;

   return (unsigned)size == packet.length ? 0 : -1;
}

PacketType Core::getPktTypeFromUserNetType(carbon_network_t net_type)
{
   switch(net_type)
   {
      case CARBON_NET_USER_1:
         return USER_1;

      case CARBON_NET_USER_2:
         return USER_2;

      default:
         LOG_PRINT_ERROR("Unrecognized User Network(%u)", net_type);
         return (PacketType) -1;
   }
}

Core::State 
Core::getState()
{
   ScopedLock scoped_lock(m_core_state_lock);
   return m_core_state;
}

void
Core::setState(State core_state)
{
   ScopedLock scoped_lock(m_core_state_lock);
   m_core_state = core_state;
}
