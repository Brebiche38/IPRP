FROM debian

RUN apt-get -y update && apt-get install -y git make gcc bc
