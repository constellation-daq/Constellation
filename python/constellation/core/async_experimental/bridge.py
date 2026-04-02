import asyncio
import threading
import time
from uuid import UUID

import zmq
import zmq.asyncio

from constellation.core.chirp import (
    CHIRPBeaconTransmitter,
    CHIRPMessageType,
    CHIRPServiceIdentifier,
)
from constellation.core.cscp import CommandTransmitter
from constellation.core.message.cmdp1 import CMDP1LogMessage, CMDP1Message, CMDP1StatMessage
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.network import get_interface_addresses

from .async_pools import AsyncSubscriberPool
from .async_heartbeat import AsyncHeartbeatReceiver


class CombinedBridge:
    """Combined async bridge - CHIRP discovery (thread), heartbeat tracking, CMDP receiving, and CSCP commands (asyncio)."""

    def __init__(self, group: str, loop: asyncio.AbstractEventLoop):
        self.group = group
        self._loop = loop

        # Async ZMQ context
        self._ctx = zmq.asyncio.Context()
        # Sync ZMQ context for CSCP (REQ/REP is blocking)
        self._sync_ctx = zmq.Context()

        self._stop = asyncio.Event()
        self._stop_thread = threading.Event()

        # Async components
        self._heartbeat = AsyncHeartbeatReceiver(
            self._ctx,
            on_state_change=self._on_state_change,
            on_satellite_dead=self._on_satellite_dead,
        )
        self._cmdp_pool = AsyncSubscriberPool(self._ctx)

        # CSCP transmitters (for commands, keyed by canonical name)
        self._transmitters: dict[str, CommandTransmitter] = {}
        self._transmitter_lock = threading.Lock()

        # Track discovered services
        self._discovered: dict[UUID, dict] = {}

        # Subscribe to logs at INFO+ and all stats
        self._cmdp_pool.set_topics([
            "LOG/INFO",
            "LOG/WARNING",
            "LOG/STATUS",
            "LOG/CRITICAL",
            "STAT/",
        ])

        # Stats
        self.stats = {
            "state_changes": 0,
            "logs": 0,
            "metrics": 0,
            "commands_sent": 0,
        }

    def start(self) -> None:
        """Start the CHIRP discovery thread."""
        self._chirp_thread = threading.Thread(
            target=self._chirp_loop,
            daemon=True,
        )
        self._chirp_thread.start()

    async def run(self) -> None:
        """Run all async components concurrently."""
        await asyncio.gather(
            self._heartbeat.run(self._stop),
            self._cmdp_pool.run(self._on_cmdp_message, self._stop),
        )

    async def send_command(self, canonical_name: str, cmd: str, payload=None) -> str:
        """Send a CSCP command via asyncio.to_thread."""
        def _send():
            with self._transmitter_lock:
                ct = self._transmitters.get(canonical_name)
            if ct is None:
                return f"ERROR: No transmitter for {canonical_name}"
            try:
                msg = ct.request_get_response(command=cmd, payload=payload)
                return f"{msg.verb}: {msg.verb_msg}"
            except Exception as e:
                return f"ERROR: {e}"

        self.stats["commands_sent"] += 1
        result = await asyncio.to_thread(_send)
        return result

    def shutdown(self) -> None:
        """Stop everything."""
        self._stop.set()
        self._stop_thread.set()
        if hasattr(self, '_chirp_thread'):
            self._chirp_thread.join(timeout=2)
        self._heartbeat.close()
        self._cmdp_pool.close()
        # Close CSCP transmitters
        with self._transmitter_lock:
            for ct in self._transmitters.values():
                ct.close()
        self._ctx.term()
        self._sync_ctx.term()

    def _on_state_change(self, name, old_state, new_state):
        self.stats["state_changes"] += 1
        print(f"  [STATE]  {name}: {old_state.name} -> {new_state.name}")

    def _on_satellite_dead(self, name):
        print(f"  [DEAD]   {name} stopped sending heartbeats")

    def _on_cmdp_message(self, frames: list[bytes]):
        try:
            msg = CMDP1Message.disassemble(frames)
            if msg.is_log_message():
                log_msg = CMDP1LogMessage.from_cmdp_message(msg)
                record = log_msg.to_log_record()
                level = record.levelname
                sender = getattr(record, 'sender', record.name)
                message = record.getMessage()
                self.stats["logs"] += 1
                print(f"  [LOG]    [{level:8s}] {sender}: {message}")
            elif msg.is_stat_message():
                stat_msg = CMDP1StatMessage.from_cmdp_message(msg)
                self.stats["metrics"] += 1
                print(f"  [STAT]   {stat_msg.metric.name} = {stat_msg.value}{stat_msg.metric.unit}")
        except Exception as e:
            pass  # Silently discard decode errors

    def _chirp_loop(self):
        """Discover HEARTBEAT, MONITORING, and CONTROL services."""
        from constellation.core.chirp import get_uuid

        beacon = CHIRPBeaconTransmitter(
            "CombinedBridge.Bridge",
            self.group,
            get_interface_addresses(None),
        )

        # Request all service types
        beacon.emit(CHIRPServiceIdentifier.HEARTBEAT, CHIRPMessageType.REQUEST)
        time.sleep(0.1)
        beacon.emit(CHIRPServiceIdentifier.MONITORING, CHIRPMessageType.REQUEST)
        time.sleep(0.1)
        beacon.emit(CHIRPServiceIdentifier.CONTROL, CHIRPMessageType.REQUEST)

        while not self._stop_thread.is_set():
            msg = beacon.listen()
            if msg is None:
                continue

            if msg.msgtype == CHIRPMessageType.REQUEST:
                continue

            if msg.msgtype != CHIRPMessageType.OFFER:
                continue

            uuid = msg.host_uuid
            address = msg.from_address
            port = msg.port
            service = msg.serviceid

            # Track what we've discovered per host
            if uuid not in self._discovered:
                self._discovered[uuid] = {}

            if service in self._discovered[uuid]:
                continue  # Already discovered

            self._discovered[uuid][service] = (address, port)

            if service == CHIRPServiceIdentifier.HEARTBEAT:
                print(f"  [CHIRP]  HEARTBEAT at {address}:{port}")
                self._loop.call_soon_threadsafe(
                    self._heartbeat.add_satellite,
                    uuid, address, port,
                    f"Unknown-{str(uuid)[:8]}",
                )

            elif service == CHIRPServiceIdentifier.MONITORING:
                print(f"  [CHIRP]  MONITORING at {address}:{port}")
                self._loop.call_soon_threadsafe(
                    self._cmdp_pool.add_socket,
                    uuid, address, port,
                )

            elif service == CHIRPServiceIdentifier.CONTROL:
                print(f"  [CHIRP]  CONTROL at {address}:{port}")
                self._setup_transmitter(uuid, address, port)

        beacon.close()

    def _setup_transmitter(self, uuid: UUID, address: str, port: int):
        """Set up CSCP transmitter for a satellite (runs in CHIRP thread)."""
        try:
            socket = self._sync_ctx.socket(zmq.REQ)
            socket.connect(f"tcp://{address}:{port}")
            socket.setsockopt(zmq.LINGER, 2000)
            socket.setsockopt(zmq.RCVTIMEO, 5000)

            ct = CommandTransmitter("CombinedBridge.Bridge", socket)

            # Get canonical name
            msg = ct.request_get_response("get_commands")
            canonical_name = msg.sender

            with self._transmitter_lock:
                self._transmitters[canonical_name] = ct

            print(f"  [CSCP]   Connected to {canonical_name}")
        except Exception as e:
            print(f"  [CSCP]   Failed to connect: {e}")


async def main():
    group = "test"

    loop = asyncio.get_running_loop()
    bridge = CombinedBridge(group, loop)

    print(f"Combined Bridge for group '{group}'")
    print(f"Architecture: 1 thread (CHIRP) + asyncio event loop")
    print()

    bridge.start()

    # Wait for discovery
    await asyncio.sleep(2)

    # Run async components in background
    run_task = asyncio.create_task(bridge.run())

    # Wait for a satellite to be discovered
    print("Waiting for satellite discovery...")
    for _ in range(20):
        if bridge._transmitters:
            break
        await asyncio.sleep(0.5)

    if not bridge._transmitters:
        print("No satellites found. Make sure PyRandomTransmitter is running.")
        bridge.shutdown()
        return

    sat_name = list(bridge._transmitters.keys())[0]
    print(f"\nFound: {sat_name}")
    print(f"Current states: {bridge._heartbeat.states}")
    print()

    # Send commands through the bridge
    print("--- Sending commands ---")

    result = await bridge.send_command(sat_name, "initialize", {})
    print(f"  initialize -> {result}")
    await asyncio.sleep(2)
    print(f"  States: {bridge._heartbeat.states}")

    result = await bridge.send_command(sat_name, "launch")
    print(f"  launch -> {result}")
    await asyncio.sleep(2)
    print(f"  States: {bridge._heartbeat.states}")

    result = await bridge.send_command(sat_name, "land")
    print(f"  land -> {result}")
    await asyncio.sleep(2)
    print(f"  States: {bridge._heartbeat.states}")

    # Let it run a bit more to collect remaining messages
    await asyncio.sleep(3)

    # Shutdown
    bridge.shutdown()
    run_task.cancel()
    try:
        await run_task
    except asyncio.CancelledError:
        pass

    print()
    print("--- Results ---")
    print(f"  State changes detected: {bridge.stats['state_changes']}")
    print(f"  Log messages received:  {bridge.stats['logs']}")
    print(f"  Metrics received:       {bridge.stats['metrics']}")
    print(f"  Commands sent:          {bridge.stats['commands_sent']}")
    print(f"  Architecture:           1 thread (CHIRP) + asyncio event loop")


if __name__ == "__main__":
    asyncio.run(main())
