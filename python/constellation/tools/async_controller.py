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
from constellation.core.async_experimental.async_controller import AsyncCLIController
from constellation.core.base import EPILOG, ConstellationArgumentParser
from constellation.core.controller import ControllerState
from constellation.core.controller_configuration import load_config


def main(args=None) -> None:
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument("-c", "--config", type=str, help="Path to the configuration file to load.")

    args = vars(parser.parse_args(args))

    cfg_file = args.pop("config")
    log_level = args.pop("level")

    ctrl = AsyncCLIController(log_level=log_level, **args)

    constellation = ctrl.constellation

    print("\nWelcome to the Constellation Async CLI Controller!\n")
    print("You can interact with the discovered Satellites via the `constellation` array:")
    print("         > constellation.get_state()\n")
    print("To get help for any of its methods, call it with a question mark:")
    print("         > constellation.get_state?\n")

    if cfg_file:
        cfg = load_config(cfg_file)  # noqa: F841
        print(f"The configuration file '{cfg_file}' has been loaded into 'cfg'.\n")

    print("   Happy hacking! :)\n")

    class ControllerPrompt(Prompts):
        """Customized prompt."""

        def in_prompt_tokens(self, _cli=None):
            return [
                (Token, ""),
                (Token.Generic.Subheading, "\U0001f4e1 v"),
                (Token.Generic.Subheading, __version__),
                (Token.Generic.Subheading, " ("),
                (Token.Generic.Subheading, __version_code_name__),
                (Token.Generic.Subheading, ")"),
                (Token, " "),
                (Token.Prompt, "\U0001f6f0 "),
                (Token.Prompt, str(len(constellation.satellites))),
                (Token, " "),
                (Token.Name.Class, ctrl.state.emoji + " " + ctrl.state.name),  # type: ignore[attr-defined]
                (Token, " "),
                (Token.Name.Entity, "async"),
                (Token, "\n"),
                (
                    (
                        Token.Prompt
                        if self.shell.last_execution_succeeded and ctrl.state not in [ControllerState.ERROR]
                        else Token.Generic.Error
                    ),
                    f"{ctrl.group} \u276f ",
                ),
            ]

        def out_prompt_tokens(self, _cli=None):
            return []

    ipython_cfg = Config()
    ipython_cfg.InteractiveShell.enable_tip = False
    ipython_cfg.TerminalInteractiveShell.prompts_class = ControllerPrompt
    ipshell = InteractiveShellEmbed(
        config=ipython_cfg,
        banner1="Starting Async IPython Controller for Constellation",
        exit_msg="Have a nice day!",
    )

    rich.pretty.install()

    ipshell()

    ctrl.reentry()


if __name__ == "__main__":
    main()
