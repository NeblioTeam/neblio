# left here just to store the changes which are needed to be made to the root container
FROM neblioteam/nebliod-build-ccache

RUN apt update
# install project dependencies
RUN apt install -y --no-install-recommends libudev-dev libusb-1.0-0-dev libhidapi-dev

# the following should be unnecessary once the root container is modified
WORKDIR /root
RUN rm -rf boost_build
RUN git clone -b master https://github.com/NeblioTeam/neblio 
RUN python neblio/build_scripts/CompileBoost-Linux.py
RUN rm -rf neblio
WORKDIR /