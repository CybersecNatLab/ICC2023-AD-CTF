FROM debian:bookworm

RUN apt-get update && apt-get install -y libvirt-clients python3 python3-pip \
	pipenv socat openssh-client sshpass

WORKDIR /chall
COPY ./Pipfile ./Pipfile.lock /chall/
RUN pipenv install

ENTRYPOINT ["pipenv", "run"]
