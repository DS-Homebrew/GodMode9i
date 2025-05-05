FROM devkitpro/devkitarm:20241104
RUN apt-get update
RUN git config --global --add safe.directory '*'
WORKDIR /data