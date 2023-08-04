FROM python:3.11-alpine

RUN pip install pipenv

RUN adduser --disabled-password --gecos '' user
WORKDIR /home/user

COPY ./Pipfile ./Pipfile.lock /home/user
RUN pipenv install

ENTRYPOINT ["pipenv", "run"]
