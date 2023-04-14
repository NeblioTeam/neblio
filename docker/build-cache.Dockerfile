FROM neblioteam/nebliod-build-ccache

RUN apt update
# install project dependencies
RUN apt install -y --no-install-recommends libudev-dev libusb-1.0-0-dev libhidapi-dev

WORKDIR /root
RUN rm -rf boost_build
RUN git clone -b fix-build-v2 https://github.com/vacuumlabs/neblio 
RUN python neblio/build_scripts/CompileBoost-Linux.py
RUN rm -rf neblio
WORKDIR /