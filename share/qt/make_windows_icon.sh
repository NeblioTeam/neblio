#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/neblio.ico

convert ../../src/qt/res/icons/neblio-16.png ../../src/qt/res/icons/neblio-32.png ../../src/qt/res/icons/neblio-48.png ${ICON_DST}
