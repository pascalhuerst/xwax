#!/bin/bash

BASEDIR=$(dirname "$0")
cd "$BASEDIR"/..

cargo run --bin media-player -- --file ./testdata/serato_cd_1min.wav   | pv -q -L $((44100 * 2)) |  cargo run --bin neowax-ui -- --audio-file /run/media/paso/MUMU/01-gang_star_feat._inspectah_deck-above_the_clouds-ksi\ Kopie.mp3
