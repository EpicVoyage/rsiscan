# rsiscan
Command-line stock screener.

# Usage
Usage: ./rsiscan [--switch] [--switch] [TKR1] [TKR2] [etc.]

    --google     Download data from finance.google.com
    --up=##      Only show stocks that have moved up ## percent
    --sleep=##   Sleep ## seconds between server requests [default: 5]
    --offline    Do not download any data for this run
    --walk       Walk back through the stock histories
    --low52      Stocks near their 52-week low
    --test       A test screener...
    --divergence Look for divergences in the RSI, MACD, and MACD histogram
    --tails      Look for lows outside BB, with closes inside 3 out of 4 days
    --narrow-bbands Narrow Bollinger Bands.
    --verbose    Print debug data along the way
    --help       Print this text and exit

Tickers listed on the command line will be added to the local cache and used for
future runs when you don't specify any.

We automatically filter out stocks with an average trading volume under one
million shares per day over the past two weeks.

This program will perform an analysis on the stocks you specify. It is *NOT*
advice and should not be used in place of actually talking to your broker. It
simply tries to look for anomalies in large amounts of data, but is not
guaranteed to even do that. Use at your own risk.

# Roadmap
Current Makefile only works on MacOS. Code should run on any modern *nix
OS.
