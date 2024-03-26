#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import readline
import threading
import time
from queue import Empty
from typing import Dict

import zmq

from .broadcastmanager import CHIRPBroadcaster, DiscoveredService
from .chirp import CHIRPServiceIdentifier
from .confighandler import get_config
from .cscp import CommandTransmitter
from .fsm import SatelliteFSM


class BaseCLIController(CHIRPBroadcaster):
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, *args, hosts=None, **kwargs):
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - hosts ::  name, address and port of satellites to control
        """
        super().__init__(*args, **kwargs)

        self.transmitters: Dict[str, CommandTransmitter] = {}
        self.context = zmq.Context()

        super()._add_com_thread()
        super()._start_com_threads()
        self._logger = logging.getLogger(__name__)

        if hosts:
            for host in hosts:
                self._add_satellite(host_name=host, host_addr=host)

        self.register_request(
            CHIRPServiceIdentifier.CONTROL, self._add_satellite_callback
        )
        self.request(CHIRPServiceIdentifier.CONTROL)
        self.target_host = None

    def _add_satellite_callback(
        self, _broadcaster: CHIRPBroadcaster, service: DiscoveredService
    ):
        """Callback method of add_satellite. Add satellite to command on service socket and address."""
        self._add_satellite(str(service.host_uuid), str(service.address), service.port)

    def _add_satellite(self, host_name: str, host_addr: str, port: int | None = None):
        """Add satellite socket to controller on port."""
        if "tcp://" not in host_addr[:6]:
            host_addr = "tcp://" + host_addr
        if port:
            host_addr = host_addr + ":" + str(port)
        socket = self.context.socket(zmq.REQ)
        socket.connect(host_addr)
        self.transmitters[host_name] = CommandTransmitter(host_name, socket)
        self._logger.info(
            "connecting to %s, address %s on port %s...",
            host_name,
            host_addr,
            port,
        )

    def _command_satellite(
        self, cmd: str, payload: any, meta: dict, host_name: str = None
    ):
        """Send cmd and await response."""
        try:
            ret_msg = self.transmitters[host_name].request_get_response(
                cmd,
                payload,
                meta,
            )
            return ret_msg

        except TimeoutError:
            self._logger.error(
                "Host %s did not receive response. Command timed out.",
                host_name,
            )
        except KeyError:
            self._logger.error("Invalid satellite name.")

    def command(self, msg):
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        if self.target_host:
            host_names = [self.target_host]
        else:
            host_names = self.transmitters.keys()

        for host_name in host_names:
            cmd, payload, meta = self._convert_to_cscp(msg=msg, host_name=host_name)
            self._logger.info("Host %s send command %s...", host_name, cmd)

            ret_msg = self._command_satellite(
                cmd=cmd,
                payload=payload,
                meta=meta,
                host_name=host_name,
            )
            self._logger.info(
                "Host %s received response: %s, %s",
                host_name,
                ret_msg.msg_verb,
                ret_msg.msg,
            )
            if ret_msg.header_meta:
                self._logger.info("    header: %s", ret_msg.header_meta)
            if ret_msg.payload:
                self._logger.info("    payload: %s", ret_msg.payload)

    def _convert_to_cscp(self, msg, host_name):
        """Convert command string into CSCP message, payload and meta."""
        cmd = msg[0]
        payload = msg[:-1]
        meta = None

        if cmd == "initialize" or cmd == "reconfigure":
            config_path = msg[1]
            class_msg = self._command_satellite("get_class", None, None, host_name)

            payload = {}

            for category in ["constellation", "satellites"]:
                try:
                    payload.update(
                        get_config(
                            config_path=config_path,
                            category=category,
                            host_class=class_msg.msg,
                            host_device="powersupply1",  # TODO: generalize
                        )
                    )
                except KeyError as e:
                    self.log.warning("Configuration file does not contain key %s", e)
        # TODO: add more commands?
        return cmd, payload, meta

    def process_cli_command(self, user_input):
        """Process CLI input commands. If not part of CLI keywords, assume it is a command for satellite."""
        if user_input.startswith("target"):
            target = user_input.split(" ")[1]
            if target in self.transmitters.keys():
                self.target_host = target
                self._logger.info(f"target for next command: host {self.target_host}")
            else:
                self._logger.error(f"No host {target}")

        elif user_input.startswith("untarget"):
            self.target_host = None

        elif user_input.startswith("add"):
            satellite_info = user_input.split(" ")
            host_name = str(satellite_info[1])
            host_addr = str(satellite_info[2])
            port = str(satellite_info[3])
            self._add_satellite(host_name=host_name, host_addr=host_addr, port=port)

        elif user_input.startswith("remove"):
            target = user_input.split(" ")[1]
            if target in self.transmitters.keys():
                self.transmitters.pop(target)
            else:
                self._logger.error(f"No host {target}")
        else:
            msg = user_input.split(" ")
            self.command(msg=msg)

    def _run_task_handler(self):
        """Event loop for task handler-routine"""
        while not self._task_handler_event.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    self.log.error("Caught exception handling task: %s", repr(e))
            except Empty:
                # nothing to process
                pass

    def run(self):
        """Run controller."""
        self.command("get_state")
        self.command("transition initialize")
        self.command("transition prepare")
        self.command("transition start_run")
        self.command("get_state")

    def run_from_cli(self):
        """Run commands from CLI and pass them to task handler-routine."""
        print(
            'Possible commands: "exit", "get_state", "<transition>", "target <uuid>", \
            "failure", "register <ip> <port>", "add <ip> <port>", "remove <uuid>"'
        )
        print(
            'Possible transitions: "initialize", "load", "unload", "launch", "land", \
            "start", "stop", "recover", "reset"'
        )
        self._task_handler_event = threading.Event()
        task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        task_handler_thread.start()
        time.sleep(0.5)
        while True:
            user_input = input("Send command: ")
            if user_input == "exit":
                self._stop_com_threads()
                self._task_handler_event.set()
                task_handler_thread.join()
                break
            else:
                self.task_queue.put([self.process_cli_command, [user_input]])
            time.sleep(0.5)


class SatelliteManager(BaseCLIController):
    """Satellite Manager class implementing CHIRP protocol"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


class CliCompleter(object):  # Custom completer
    def __init__(self, commands, transitions):
        self.commands = sorted(commands)
        self.transitions = sorted(transitions)

    # Returns the first word if there is a space, otherwise nothing
    def get_cur_before(self):
        idx = readline.get_begidx()
        full = readline.get_line_buffer()
        prefix = full[:idx]
        n = prefix.split()
        if len(n) > 0:
            return n[0]
        else:
            return ""

    def complete(self, text, state):
        cmd = self.get_cur_before()
        if cmd == "transition":
            return self.complete_transition(text, state)
        elif cmd != "":
            return None
        if text == "":  # Display all possibilities
            self.matches = self.commands[:]
        else:
            self.matches = [s for s in self.commands if s and s.startswith(text)]

        if state > len(self.matches):
            return None
        else:
            return self.matches[state]

    def complete_transition(self, text, state):
        matches = [s for s in self.transitions if s and s.startswith(text)]
        if state > len(matches):
            return None
        else:
            return matches[state]


def main():
    """Start a controller."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--satellite", "--sat", action="append")
    parser.add_argument("--name", type=str, default="controller_demo")
    parser.add_argument("--group", type=str, default="constellation")

    args = parser.parse_args()

    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )
    # if not args.satellite:
    #    print("No satellites specified! Use '--satellite' to add one.")
    #    return
    # Set up simple tab completion
    commands = [
        "exit",
        "get_state",
        "transition ",
        "failure",
        "register ",
        "add ",
        "remove ",
    ]
    transitions = [t.name for t in SatelliteFSM.events]

    cliCompleter = CliCompleter(list(set(commands)), list(set(transitions)))
    readline.set_completer_delims(" \t\n;")
    readline.set_completer(cliCompleter.complete)
    readline.parse_and_bind("tab: complete")

    # start server with args
    ctrl = BaseCLIController(name=args.name, group=args.group, hosts=args.satellite)
    ctrl.run_from_cli()
    # ctrl.run()


if __name__ == "__main__":
    main()
