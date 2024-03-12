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
from .broadcastmanager import CHIRPBroadcaster, DiscoveredService
from typing import Dict
from uuid import UUID


class BaseCLIController:
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, name: str, group: str, hosts=None):
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - hosts ::  name, address and port of satellites to control
        """
        self._logger = logging.getLogger(__name__)

        self.transmitters: Dict[UUID, CommandTransmitter] = {}
        self.context = zmq.Context()
        if hosts:
            for host in hosts:
                self.add_satellite(host_name="Example", host_addr=host)

        self.broadcast_manager = CHIRPBroadcaster(name, group, None)
        self.broadcast_manager.register_request(
            CHIRPServiceIdentifier.CONTROL, self.add_satellite
        )
        self.broadcast_manager.request(CHIRPServiceIdentifier.CONTROL)
        self.target_host = None

    def add_satellite_callback(self, service: DiscoveredService):
        socket = self.context.socket(zmq.REQ)
        socket.connect(service.address + ":" + service.port)
        self.transmitters[service.host_uuid] = CommandTransmitter(
            str(service.host_uuid), socket
        )
        self._logger.info(
            "connecting to %s, address %s...",
            service.host_uuid,
            service.address,
        )

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
            "connecting to %s, address %s...",
            host_name,
            host_addr,
        )

    def _command_satellite(self, cmd, payload, meta, host_name=None):
        """Send cmd and await response."""

        self.transmitters[host_name].send_request(
            cmd,
            payload,
            meta,
        )
        self._logger.info("Host %s send command %s...", host_name, cmd)

        try:
            msg = self.transmitters[host_name].get_message()
            self._logger.info(
                "Host %s received response: %s, %s",
                host_name,
                msg.msg_verb,
                msg.msg,
            )
            if msg.header_meta:
                self._logger.info("    header: %s", msg.header_meta)
            if payload:
                self._logger.info("    payload: %s", msg.payload)

        except TimeoutError:
            self._logger.error(
                "Host %s did not receive response. Command timed out.",
                host_name,
            )
        except KeyError:
            self._logger.error("Invalid satellite name.")

    def command(self, msg, host_name=None):
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        if host_name:
            cmd, payload, meta = self._convert_to_cscp(msg, host_name)
            self._command_satellite(
                cmd=cmd,
                payload=payload,
                meta=meta,
                host_name=host_name,
            )
        else:
            for host in self.transmitters.keys():
                cmd, payload, meta = self._convert_to_cscp(msg, host)
                self._command_satellite(
                    cmd=cmd,
                    payload=payload,
                    meta=meta,
                    host_name=host,
                )

    def _convert_to_cscp(self, msg, host_name):
        """Convert command string into CSCP message, payload and meta."""
        cmd = msg[0]
        payload = msg[:-1]
        meta = None

        if cmd == "initialize":
            config_path = msg[1]
            payload = self.get_config(host_name=host_name, config_path=config_path)
        elif cmd == "reconfigure":
            config_path = msg[1]
            if msg[2]:
                for trait in msg[2:]:
                    payload.append(
                        self.get_config(
                            host_name=host_name, config_path=config_path, trait=trait
                        )
                    )
            else:
                payload.append(
                    self.get_config(host_name=host_name, config_path=config_path)
                )
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
            self.target_host = target

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
            msg = user_input.split(" ")
            self.command(
                msg=msg,
                host_name=self.target_host,
            )

    def get_config(
        self,
        host_name: str,
        config_path: str,
        trait: str | None = None,
    ):
        """Get configuration of satellite. Specify trait to only get part of config."""
        config = read_config(config_path)

        try:
            ret_config = pack_config(config["GENERAL"])
            host_config = pack_config(config[host_name])
            ret_config.update(host_config)

            if trait:
                trait_config = filter_config(trait, host_config)
                ret_config.update(trait_config)

            return ret_config

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
                self.process_cli_command(user_input)


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
