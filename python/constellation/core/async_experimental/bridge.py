import asyncio
import threading
import time
from concurrent.futures import ThreadPoolExecutor
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
from constellation.core.network import get_interface_addresses

from .async_heartbeat import AsyncHeartbeatReceiver
from .async_pools import AsyncSubscriberPool


class CombinedBridge:
    """Combined async bridge.

    CHIRP discovery runs in a dedicated thread since it uses raw UDP multicast.
    Heartbeat tracking, CMDP receiving, and CSCP commands all run in the
    asyncio event loop.
    """

    def __init__(self, group: str, loop: asyncio.AbstractEventLoop) -> None:
        self.group = group
        self._loop = loop

        self._ctx = zmq.asyncio.Context()
        self._sync_ctx = zmq.Context()

        self._stop = asyncio.Event()
        self._stop_thread = threading.Event()

        self._heartbeat = AsyncHeartbeatReceiver(
            self._ctx,
            on_state_change=self._on_state_change,
            on_satellite_dead=self._on_satellite_dead,
        )
        self._cmdp_pool = AsyncSubscriberPool(
            self._ctx,
            callback=self._on_cmdp_message,
        )
        self._cmdp_pool.set_topics([
            "LOG/INFO",
            "LOG/WARNING",
            "LOG/STATUS",
            "LOG/CRITICAL",
            "STAT/",
        ])

        self._transmitters: dict[str, CommandTransmitter] = {}
        self._transmitter_uuids: dict[UUID, str] = {}
        self._transmitter_lock = threading.Lock()
        self._cscp_executor = ThreadPoolExecutor(max_workers=1)

        self._discovered: dict[UUID, dict] = {}

        self.stats = {
            "state_changes": 0,
            "logs": 0,
            "metrics": 0,
            "commands_sent": 0,
        }

    def start(self) -> None:
        """Start the CHIRP discovery thread."""
        self._chirp_thread = threading.Thread(target=self._chirp_loop, daemon=True)
        self._chirp_thread.start()

    async def run(self) -> None:
        """Run heartbeat and CMDP components concurrently."""
        await asyncio.gather(
            self._heartbeat.run(self._stop),
            self._cmdp_pool.run(self._stop),
        )

    async def send_command(self, canonical_name: str, cmd: str, payload=None) -> str:
        """Send a CSCP command via a dedicated single-thread executor.

        The executor serializes all REQ socket operations so concurrent
        send_command calls never interleave on the same socket.
        """
        def _send() -> str:
            # Lock protects _transmitters dict which is modified from CHIRP thread
            with self._transmitter_lock:
                ct = self._transmitters.get(canonical_name)
            if ct is None:
                return f"ERROR: No transmitter for {canonical_name}"
            try:
                msg = ct.request_get_response(command=cmd, payload=payload)
                return f"{msg.verb_msg}"
            except Exception as e:
                return f"ERROR: {e}"

        self.stats["commands_sent"] += 1
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(self._cscp_executor, _send)

    def shutdown(self) -> None:
        """Stop all components.

        Cancel and await run() before calling this to avoid terminating
        contexts while sockets are still open.
        """
        self._stop.set()
        self._stop_thread.set()
        if hasattr(self, "_chirp_thread"):
            self._chirp_thread.join(timeout=2)
        self._cscp_executor.shutdown(wait=True)
        self._heartbeat.close()
        self._cmdp_pool.close()
        with self._transmitter_lock:
            for ct in self._transmitters.values():
                ct.close()
        self._ctx.term()
        self._sync_ctx.term()

    def _on_state_change(self, name: str, old_state, new_state) -> None:
        self.stats["state_changes"] += 1
        print(f"  [STATE]  {name}: {old_state.name} -> {new_state.name}")

    def _on_satellite_dead(self, name: str) -> None:
        print(f"  [DEAD]   {name} stopped sending heartbeats")

    def _on_cmdp_message(self, uuid: UUID, frames: list[bytes]) -> None:
        try:
            msg = CMDP1Message.disassemble(frames)
            if msg.is_log_message():
                log_msg = CMDP1LogMessage.from_cmdp_message(msg)
                record = log_msg.to_log_record()
                sender = getattr(record, "sender", record.name)
                self.stats["logs"] += 1
                print(f"  [LOG]    [{record.levelname:8s}] {sender}: {record.getMessage()}")
            elif msg.is_stat_message():
                stat_msg = CMDP1StatMessage.from_cmdp_message(msg)
                self.stats["metrics"] += 1
                print(f"  [STAT]   {stat_msg.metric.name} = {stat_msg.value}{stat_msg.metric.unit}")
        except Exception:
            pass

    def _chirp_loop(self) -> None:
        """Discover and track satellites via CHIRP."""
        beacon = CHIRPBeaconTransmitter(
            "CombinedBridge.Bridge",
            self.group,
            get_interface_addresses(None),
        )

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

            uuid = msg.host_uuid
            address = msg.from_address
            port = msg.port
            service = msg.serviceid

            if msg.msgtype == CHIRPMessageType.OFFER:
                if uuid not in self._discovered:
                    self._discovered[uuid] = {}

                if service in self._discovered[uuid]:
                    continue

                self._discovered[uuid][service] = (address, port)

                if service == CHIRPServiceIdentifier.HEARTBEAT:
                    name = self._transmitter_uuids.get(uuid, f"Unknown-{str(uuid)[:8]}")
                    print(f"  [CHIRP]  HEARTBEAT from {address}:{port}")
                    self._loop.call_soon_threadsafe(
                        self._heartbeat.add_satellite,
                        uuid, address, port, name,
                    )

                elif service == CHIRPServiceIdentifier.MONITORING:
                    print(f"  [CHIRP]  MONITORING from {address}:{port}")
                    self._loop.call_soon_threadsafe(
                        self._cmdp_pool.add_socket,
                        uuid, address, port,
                    )

                elif service == CHIRPServiceIdentifier.CONTROL:
                    print(f"  [CHIRP]  CONTROL from {address}:{port}")
                    self._setup_transmitter(uuid, address, port)

            elif msg.msgtype == CHIRPMessageType.DEPART:
                if uuid not in self._discovered:
                    continue

                self._discovered[uuid].pop(service, None)

                if service == CHIRPServiceIdentifier.HEARTBEAT:
                    print(f"  [CHIRP]  DEPART HEARTBEAT {uuid}")
                    self._loop.call_soon_threadsafe(
                        self._heartbeat.remove_satellite, uuid,
                    )

                elif service == CHIRPServiceIdentifier.MONITORING:
                    print(f"  [CHIRP]  DEPART MONITORING {uuid}")
                    self._loop.call_soon_threadsafe(
                        self._cmdp_pool.remove_socket, uuid,
                    )

                elif service == CHIRPServiceIdentifier.CONTROL:
                    print(f"  [CHIRP]  DEPART CONTROL {uuid}")
                    self._cleanup_transmitter(uuid)

        beacon.close()

    def _setup_transmitter(self, uuid: UUID, address: str, port: int) -> None:
        """Set up a CSCP transmitter for a satellite (runs in CHIRP thread)."""
        socket = self._sync_ctx.socket(zmq.REQ)
        socket.connect(f"tcp://{address}:{port}")
        socket.setsockopt(zmq.LINGER, 2000)
        socket.setsockopt(zmq.RCVTIMEO, 5000)
        try:
            ct = CommandTransmitter("CombinedBridge.Bridge", socket)
            msg = ct.request_get_response("get_commands")
            canonical_name = msg.sender
            with self._transmitter_lock:
                self._transmitters[canonical_name] = ct
                self._transmitter_uuids[uuid] = canonical_name
            print(f"  [CSCP]   Connected to {canonical_name}")
        except Exception as e:
            socket.close()
            print(f"  [CSCP]   Failed to connect to {address}:{port}: {e}")

    def _cleanup_transmitter(self, uuid: UUID) -> None:
        """Remove and close a CSCP transmitter on satellite departure."""
        with self._transmitter_lock:
            canonical_name = self._transmitter_uuids.pop(uuid, None)
            if canonical_name is None:
                return
            ct = self._transmitters.pop(canonical_name, None)
        if ct is not None:
            ct.close()


async def main() -> None:
    group = "test"
    loop = asyncio.get_running_loop()
    bridge = CombinedBridge(group, loop)

    print(f"Combined Bridge for group '{group}'")
    print(f"Architecture: 1 thread (CHIRP) + asyncio event loop")
    print()

    bridge.start()
    await asyncio.sleep(2)

    run_task = asyncio.create_task(bridge.run())

    print("Waiting for satellite discovery...")
    for _ in range(20):
        if bridge._transmitters:
            break
        await asyncio.sleep(0.5)

    if not bridge._transmitters:
        print("No satellites found. Make sure PyRandomTransmitter is running.")
        bridge._stop.set()
        run_task.cancel()
        try:
            await run_task
        except asyncio.CancelledError:
            pass
        bridge.shutdown()
        return

    sat_name = list(bridge._transmitters.keys())[0]
    print(f"\nFound: {sat_name}")
    print(f"Current states: {bridge._heartbeat.states}")
    print()

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

    await asyncio.sleep(3)

    run_task.cancel()
    try:
        await run_task
    except asyncio.CancelledError:
        pass

    bridge.shutdown()

    print()
    print("--- Results ---")
    print(f"  State changes detected: {bridge.stats['state_changes']}")
    print(f"  Log messages received:  {bridge.stats['logs']}")
    print(f"  Metrics received:       {bridge.stats['metrics']}")
    print(f"  Commands sent:          {bridge.stats['commands_sent']}")
    print(f"  Architecture:           1 thread (CHIRP) + asyncio event loop")


if __name__ == "__main__":
    asyncio.run(main())
