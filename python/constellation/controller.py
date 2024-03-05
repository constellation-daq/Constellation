#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import readline

import zmq

from .chirp import CHIRPServiceIdentifier
from .confighandler import pack_config, read_config, filter_config
from .cscp import CommandTransmitter
from .fsm import SatelliteFSM
from .broadcastmanager import CHIRPBroadcastManager


class TrivialController:
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, name, group, hosts):
        """Initialize values.

        Arguments:
        - hosts :: list of ip addr and ports for satellites to control
        """
        self._logger = logging.getLogger(__name__)

        self.transmitters = {}
        self.context = zmq.Context()
        for host in hosts:
            self.add_satellite(host)

        self.broadcast_manager = CHIRPBroadcastManager(name, group, None)
        self.broadcast_manager.register_request(
            CHIRPServiceIdentifier.CONTROL, self.add_satellite
        )
        self.broadcast_manager.request(CHIRPServiceIdentifier.CONTROL)
        self.target_host = None

    def add_satellite(self, host_name, host_addr, port: int | None = None):
        """Add satellite socket to controller on port."""
        if "tcp://" not in host_addr[:6]:
            host_addr = "tcp://" + host_addr
        if port:
            host_addr = host_addr + ":" + port
        socket = self.context.socket(zmq.REQ)
        socket.connect(host_addr)
        self.transmitters[host_name] = CommandTransmitter(host_name, socket)
        self._logger.info(
            "connecting to %s, ID %s...",
            host_name,
            len(self.transmitters) - 1,
        )

    def _command_satellite(self, cmd, payload, meta, host_name=None):
        self.transmitters[host_name].send_request(
            cmd,
            payload,
            meta,
        )
        self._logger.info("Host %s send command %s...", host_name, cmd)

        try:
            response, header, payload = self.transmitters[host_name].get_message()
            self._logger.info("Host %s received response: %s", host_name, response)
            if header:
                self._logger.info("    header: %s", header)
            if payload:
                self._logger.info("    payload: %s", payload)

        except TimeoutError:
            self._logger.error(
                "Host %s did not receive response. Command timed out.",
                host_name,
            )
        except KeyError:
            self._logger.error("Invalid satellite name.")

    def command(self, cmd, idx=0, host=None):
        """Send cmd and await response."""

        if host:
            self._command_satellite(cmd, idx, host)
        else:
            for sat in enumerate(self.transmitters):
                sat.send_request(cmd)

    def process_command(self, user_input):

        if user_input.startswith("target"):
            target = user_input.split(" ")[1]
            if target in self.transmitters.keys():
                self.target_host = target
                self._logger.info(f"target for next command: host {self.target_host}")
            else:
                self._logger.error(f"No host {target}")

        elif user_input.startswith("add"):
            satellite_info = user_input.split(" ")
            host_name = str(satellite_info[1])
            host_addr = str(satellite_info[2])
            port = str(satellite_info[3])
            self.add_satellite(host_name=host_name, host_addr=host_addr, port=port)

        elif user_input.startswith("remove"):
            target = user_input.split(" ")[1]
            if target in self.transmitters.keys():
                self.transmitters.pop(target)
            else:
                self._logger.error(f"No host {target}")
        else:
            if self.target_host:
                self.command(user_input)
            else:
                self.command(user_input)

    def get_config(
        self,
        host: str,
        config_path: str,
        trait: str | None = None,
    ):
        """Get configuration to satellite. Specify trait to only send part of config."""
        config = read_config(config_path)

        try:
            general_config = pack_config(config["GENERAL"])
            host_config = pack_config(config[host])

            if trait:
                trait_config = filter_config(trait, host_config)
                return trait_config, general_config

            return host_config, general_config

        except KeyError:
            self._logger.warning("Config doesn't contain specified arguments")

    def run(self):
        """Run controller."""
        self.command("get_state")
        self.command("transition initialize")
        self.command("transition prepare")
        self.command("transition start_run")
        self.command("get_state")

    def run_from_cli(self):
        """Run commands from CLI."""
        print(
            'Possible commands: "exit", "get_state", "transition <transition>", "target <id no.>", "failure", "register <ip> <port>", "add <ip> <port>", "remove <id no.>"'
        )
        print(
            'Possible transitions: "initialize", "load", "unload", "launch", "land", "start", "stop", "recover", "reset"'
        )
        while True:
            user_input = input("Send command: ")
            if user_input == "exit":
                break
            else:
                self.process_command(user_input)


class SatelliteManager(TrivialController):
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
    args = parser.parse_args()

    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )
    if not args.satellite:
        print("No satellites specified! Use '--satellite' to add one.")
        return
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
    ctrl = TrivialController(hosts=args.satellite)
    ctrl.run_from_cli()
    # ctrl.run()


if __name__ == "__main__":
    main()
