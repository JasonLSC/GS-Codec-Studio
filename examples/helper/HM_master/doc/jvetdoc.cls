%%
%% jvetdoc: Copyright (c) 2011 BBC Research & Development
%% All rights reserved.
%%
%% Permission to use, copy, modify, and/or distribute this software for any
%% purpose with or without fee is hereby granted, provided that the above
%% copyright notice and this permission notice appear in all copies.
%%
%% Neither the names of the BBC, ITU, ISO, IEC nor the names of its
%% contributors may be used to endorse or promote products derived from
%% this software without specific prior written permission.
%%
%% THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
%% WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
%% MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
%% ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
%% WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
%% ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
%% OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
%%
\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{jvetdoc}[2019/01/31 JVET document class]

\LoadClassWithOptions{article}

% typeset using Times New Roman
\RequirePackage{ifxetex}
\ifxetex
  % use the correct fonts when using XeTeX
  \RequirePackage{fontspec}
  \defaultfontfeatures{Mapping=tex-text}
  \setmainfont{Times New Roman}
\else
  \RequirePackage{times}
\fi

% expand the margins
\RequirePackage{geometry}
\geometry{tmargin=1.6cm,lmargin=1in,rmargin=2.5cm,bmargin=1in,nohead}

% require graphicx for loading the logos in the header
\RequirePackage{graphicx}

% don't number the abstract
\renewenvironment{abstract}{\section*{Abstract}}{}

% format the date in iso style
\RequirePackage{datetime}
\renewcommand{\dateseparator}{-}
\newdateformat{JVET@yyyymmdddate}{%
	\THEYEAR\dateseparator\twodigit{\THEMONTH}\dateseparator\twodigit{\THEDAY}}

% all pages only have a footer
\RequirePackage{fancyhdr}
\pagestyle{fancy}
\renewcommand{\headrulewidth}{0pt}
\fancyhead{}
\cfoot{\thepage}
\rfoot{Date saved: \JVET@yyyymmdddate\today}

% macros for document metadata
\RequirePackage[normalem]{ulem}
\def\@jvetdocnum{\uline{xxx}}
\def\@jvetdocstatus{\uline{TODO: Add \texttt{\textbackslash{}jvetdocstatus}}}
\def\@jvetdocpurpose{\uline{TODO: Add \texttt{\textbackslash{}jvetdocpurpose}}}
\def\@jvetdocsource{\uline{TODO: Add \texttt{\textbackslash{}jvetdocsource}}}
\def\@jvetmeeting{\uline{TODO: Add \texttt{\textbackslash{}jvetmeeting}}}
\newcommand{\jvetdocnum}[1]{\def\@jvetdocnum{\mbox{#1}}}
\newcommand{\jvetdocstatus}[1]{\def\@jvetdocstatus{\mbox{#1}}}
\newcommand{\jvetdocpurpose}[1]{\def\@jvetdocpurpose{\mbox{#1}}}
\newcommand{\jvetdocsource}[1]{\def\@jvetdocsource{\mbox{#1}}}
\newcommand{\jvetmeeting}[1]{\def\@jvetmeeting{\mbox{#1}}}

% affiliation block in the author list
\newenvironment{affiliation}{%
	\begin{tabular}[t]{@{}>{\renewcommand{\\}{, }}l@{}}%
}{%
	\end{tabular}%
}

\RequirePackage{array}

\RequirePackage{ifthen}
\newcounter{jvet@author@column}
\newcommand{\@jvet@switch@author@column}{%
	\ifthenelse{\value{jvet@author@column} = 1}{%
		% do nothing
	}{%
		\addtocounter{jvet@author@column}{1}%
		&
	}%
}

\newcommand*{\email}[1]{\@jvet@switch@author@column\textit{#1}}
\newcommand*{\tel}[1]{\@jvet@switch@author@column\textrm{#1}}

% Make a title at the top of the current page.
\renewcommand\maketitle{%
	% none of the title is put in the header (it is too difficult to
	% make the page match the word style in this case)
	\vspace*{-1.7cm}
	\begin{raggedright}
	\rule{0pt}{0.74cm}% strut incase images aren't loaded
	\IfFileExists{logos/itu}{\includegraphics[height=0.74cm]{logos/itu}}{}
	\IfFileExists{logos/iso}{\includegraphics[height=0.74cm]{logos/iso}}{}
	\IfFileExists{logos/iec}{\includegraphics[height=0.74cm]{logos/iec}}{}
	\\
	\textbf{Joint Video Experts Team (JVET)}\\[0ex]
	\textbf{of ITU-T SG16 WP3 and ISO/IEC JTC1/SC29/WG5}
		\hfill Document: JVET-\@jvetdocnum\\[0ex]
	\@jvetmeeting

	\vspace{0.5\baselineskip}

	\newcommand{\@strutb}{\rule{0pt}{2.5ex}}
	\begin{tabular}{lp{0.78\textwidth}}
	\@strutb \it Title: & \@title \\
	\@strutb \it Status: & \@jvetdocstatus \\
	\@strutb \it Purpose: & \@jvetdocpurpose \\
	\@strutb \it Editors: & %
		\setcounter{jvet@author@column}{0}
		\let\@and\\
		\renewcommand{\and}{\@and\setcounter{jvet@author@column}{0}}
		\newcommand{\@NLtoPAR}{\renewcommand{\\}{\par}}
		\begin{tabular}[t]{@{}>{\@NLtoPAR}p{3in}|>{\@NLtoPAR}p{2in}@{}}%
			\@author
		\end{tabular}\\
	\@strutb \it Source: & \@jvetdocsource \\
	\end{tabular}
	\end{raggedright}

	% draw a short horizontal line to delimit the title from body
	{\center\rule{0.35\textwidth}{1pt}\\}
}

%
\RequirePackage{parskip}
\RequirePackage[compact]{titlesec}
%\titlespacing{\section}{0pt}{*0}{*0}
%\titlespacing{\subsection}{0pt}{*0}{*0}
%\titlespacing{\subsubsection}{0pt}{*0}{*0}

\titlespacing*{\section}
{0pt}{5ex}{2ex}
\titlespacing*{\subsection}
{0pt}{5ex}{2ex}
\titlespacing*{\subsection}
{0pt}{5ex}{2ex}
