# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

preamble = r"""
\addto\captionsenglish{\renewcommand{\contentsname}{Contents}}
\DeclareRobustCommand{\and}{\end{tabular}\kern-\tabcolsep\\\begin{tabular}[t]{r}}
\makeatletter
  \fancypagestyle{normal}{
    \fancyhf{}
    \fancyfoot[LE,RO]{{\py@HeaderFamily\thepage}}
    \fancyhead[LE]{{\py@HeaderFamily \@title, \py@release}}
    \fancyhead[RO]{{\py@HeaderFamily \nouppercase{\leftmark}}}
    \renewcommand{\headrulewidth}{0.4pt}
    \renewcommand{\footrulewidth}{0pt}
  }
  \fancypagestyle{plain}{
    \fancyhf{}
    \fancyfoot[LE,RO]{{\py@HeaderFamily\thepage}}
    \renewcommand{\headrulewidth}{0pt}
    \renewcommand{\footrulewidth}{0pt}
  }
\makeatother
"""

maketitle = r"""
\let\oldrule\rule
\renewcommand{\rule}[2]{}
\sphinxmaketitle
\renewcommand{\rule}[2]{\oldrule{#1}{#2}}
"""
