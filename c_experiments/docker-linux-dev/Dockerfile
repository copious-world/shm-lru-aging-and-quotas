# our local base image
FROM debian:bookworm

LABEL description="Container for use with Visual Studio" 


CMD ["bash"]

RUN set -eux; \
    apt update; \
    apt install -y --no-install-recommends ca-certificates;

RUN set -eux; \
    echo "deb [trusted=yes] https://nginx.org/packages/mainline/debian/ bookworm nginx" >> /etc/apt/sources.list.d/nginx.list;

RUN set -eux; \
    apt update;

# install build dependencies 
RUN apt-get update && apt-get install -y g++ rsync zip openssh-server make 

# configure SSH for communication with Visual Studio 
RUN mkdir -p /var/run/sshd

RUN echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config && \ 
   ssh-keygen -A 

# expose port 22 
EXPOSE 22

