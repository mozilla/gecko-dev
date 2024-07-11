#! /usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This scripts plots graphs produced by our drift correction code.
#
# Install dependencies with:
#   > pip install bokeh pandas
#
# Generate the csv data file with the DriftControllerGraphs log module:
#   > MOZ_LOG=raw,sync,DriftControllerGraphs:5 \
#   > MOZ_LOG_FILE=/tmp/driftcontrol.csv       \
#   > ./mach gtest '*AudioDrift*StepResponse'
#
# Generate the graphs with this script:
#   > ./dom/media/driftcontrol/plot.py /tmp/driftcontrol.csv.moz_log
#
# The script should produce a file plot.html in the working directory and
# open it in the default browser.

import argparse
from collections import OrderedDict

import pandas
from bokeh.io import output_file, show
from bokeh.layouts import gridplot
from bokeh.models import TabPanel, Tabs
from bokeh.plotting import figure


def main():
    parser = argparse.ArgumentParser(
        prog="plot.py for DriftControllerGraphs",
        description="""Takes a csv file of DriftControllerGraphs data
(from a single DriftController instance) and plots
them into plot.html in the current working directory.

The easiest way to produce the data is with MOZ_LOG:
MOZ_LOG=raw,sync,DriftControllerGraphs:5 \
MOZ_LOG_FILE=/tmp/driftcontrol.csv       \
./mach gtest '*AudioDrift*StepResponse'""",
    )
    parser.add_argument("csv_file", type=str)
    args = parser.parse_args()

    all_df = pandas.read_csv(args.csv_file)

    # Filter on distinct ids to support multiple plotting sources
    tabs = []
    for id in list(OrderedDict.fromkeys(all_df["id"])):
        df = all_df[all_df["id"] == id]

        t = df["t"]
        buffering = df["buffering"]
        avgbuffered = df["avgbuffered"]
        desired = df["desired"]
        buffersize = df["buffersize"]
        inlatency = df["inlatency"]
        outlatency = df["outlatency"]
        inframesavg = df["inframesavg"]
        outframesavg = df["outframesavg"]
        inrate = df["inrate"]
        outrate = df["outrate"]
        steadystaterate = df["steadystaterate"]
        nearthreshold = df["nearthreshold"]
        corrected = df["corrected"]
        hysteresiscorrected = df["hysteresiscorrected"]
        configured = df["configured"]

        output_file("plot.html")

        fig1 = figure()
        # Variables with more variation are plotted after smoother variables
        # because latter variables are drawn on top and so visibility of
        # individual values in the variables with more variation is improved
        # (when both variables are shown).
        fig1.line(
            t, inframesavg, color="violet", legend_label="Average input packet size"
        )
        fig1.line(
            t, outframesavg, color="purple", legend_label="Average output packet size"
        )
        fig1.line(t, inlatency, color="hotpink", legend_label="In latency")
        fig1.line(t, outlatency, color="firebrick", legend_label="Out latency")
        fig1.line(t, desired, color="goldenrod", legend_label="Desired buffering")
        fig1.line(
            t, avgbuffered, color="orangered", legend_label="Average buffered estimate"
        )
        fig1.line(t, buffering, color="dodgerblue", legend_label="Actual buffering")
        fig1.line(t, buffersize, color="seagreen", legend_label="Buffer size")
        fig1.varea(
            t,
            [d - h for (d, h) in zip(desired, nearthreshold)],
            [d + h for (d, h) in zip(desired, nearthreshold)],
            alpha=0.2,
            color="goldenrod",
            legend_label='"Near" band (won\'t reduce desired buffering outside)',
        )

        slowConvergenceSecs = 30
        adjustmentInterval = 1
        slowHysteresis = 1
        avgError = avgbuffered - desired
        absAvgError = [abs(e) for e in avgError]
        slow_offset = [e / slowConvergenceSecs - slowHysteresis for e in absAvgError]
        fast_offset = [e / adjustmentInterval for e in absAvgError]
        low_offset, high_offset = zip(
            *[
                (s, f) if e >= 0 else (-f, -s)
                for (e, s, f) in zip(avgError, slow_offset, fast_offset)
            ]
        )

        fig2 = figure(x_range=fig1.x_range)
        fig2.varea(
            t,
            steadystaterate + low_offset,
            steadystaterate + high_offset,
            alpha=0.2,
            color="goldenrod",
            legend_label="Deadband (won't change in rate within)",
        )
        fig2.line(t, inrate, color="hotpink", legend_label="Nominal in sample rate")
        fig2.line(t, outrate, color="firebrick", legend_label="Nominal out sample rate")
        fig2.line(
            t,
            steadystaterate,
            color="orangered",
            legend_label="Estimated in rate with drift",
        )
        fig2.line(
            t, corrected, color="dodgerblue", legend_label="Corrected in sample rate"
        )
        fig2.line(
            t,
            hysteresiscorrected,
            color="seagreen",
            legend_label="Hysteresis-corrected in sample rate",
        )
        fig2.line(
            t, configured, color="goldenrod", legend_label="Configured in sample rate"
        )

        fig1.legend.location = "top_left"
        fig2.legend.location = "top_right"
        for fig in (fig1, fig2):
            fig.legend.background_fill_alpha = 0.6
            fig.legend.click_policy = "hide"

        tabs.append(TabPanel(child=gridplot([[fig1, fig2]]), title=str(id)))

    show(Tabs(tabs=tabs))


if __name__ == "__main__":
    main()
