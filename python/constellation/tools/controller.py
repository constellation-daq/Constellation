"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import rich.pretty
from IPython.terminal.embed import InteractiveShellEmbed
from IPython.terminal.prompts import Prompts
from pygments.token import Token
from traitlets.config.loader import Config

from constellation.core import __version__, __version_code_name__
from constellation.core.base import EPILOG, ConstellationArgumentParser
from constellation.core.controller import BaseController, ControllerState
from constellation.core.controller_configuration import load_config
from constellation.core.logging import setup_cli_logging


def main(args=None) -> None:
    # Add argument specific for Controller to parser
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument("-c", "--config", type=str, help="Path to the configuration file to load.")

    # Get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # Pop argument specific for Controller
    cfg_file = args.pop("config")

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start controller
    ctrl = BaseController(**args)

    # Add constellation shorthand
    constellation = ctrl.constellation

    print("\nWelcome to the Constellation CLI IPython Controller!\n")
    print("You can interact with the discovered Satellites via the `constellation` array:")
    print("         > constellation.get_state()\n")
    print("To get help for any of its methods, call it with a question mark:")
    print("         > constellation.get_state?\n")

    if cfg_file:
        # make configuration available to the user
        cfg = load_config(cfg_file)  # noqa: F841
        print(f"The configuration file '{cfg_file}' has been loaded into 'cfg'.\n")

    print("   Happy hacking! :)\n")

    #  ___ ____        _   _                            _
    # |_ _|  _ \ _   _| |_| |__   ___  _ __    ___  ___| |_ _   _ _ __
    #  | || |_) | | | | __| '_ \ / _ \| '_ \  / __|/ _ \ __| | | | '_ \
    #  | ||  __/| |_| | |_| | | | (_) | | | | \__ \  __/ |_| |_| | |_) |
    # |___|_|    \__, |\__|_| |_|\___/|_| |_| |___/\___|\__|\__,_| .__/
    #            |___/                                           |_|

    class ControllerPrompt(Prompts):
        """Customized prompt."""

        def in_prompt_tokens(self, _cli=None):
            return [
                (Token, ""),
                # show version
                (Token.Generic.Subheading, "📡 v"),
                (Token.Generic.Subheading, __version__),
                (Token.Generic.Subheading, " ("),
                (Token.Generic.Subheading, __version_code_name__),
                (Token.Generic.Subheading, ")"),
                (Token, " "),
                # show number of satellites
                (Token.Prompt, "🛰 "),
                (Token.Prompt, str(len(constellation.satellites))),
                # show current state
                (Token, " "),
                (Token.Name.Class, ctrl.state.emoji + " " + ctrl.state.name),  # type: ignore[attr-defined]
                (Token, " "),
                (Token.Name.Entity, "ipython"),
                (Token, "\n"),
                (
                    (
                        Token.Prompt
                        if self.shell.last_execution_succeeded and ctrl.state not in [ControllerState.ERROR]
                        else Token.Generic.Error
                    ),
                    f"{ctrl.group} ❯ ",
                ),
            ]

        def out_prompt_tokens(self, _cli=None):
            return []

    ipython_cfg = Config()
    ipython_cfg.InteractiveShell.enable_tip = False
    ipython_cfg.TerminalInteractiveShell.prompts_class = ControllerPrompt
    # Now create an instance of the embeddable shell. The first argument is a
    # string with options exactly as you would type them if you were starting
    # IPython at the system command line. Any parameters you want to define for
    # configuration can thus be specified here.
    ipshell = InteractiveShellEmbed(
        config=ipython_cfg,
        banner1="Starting IPython Controller for Constellation",
        exit_msg="Have a nice day!",
    )

    # Install Rich REPL
    rich.pretty.install()

    # Start IPython shell
    ipshell()


if __name__ == "__main__":
    main()
