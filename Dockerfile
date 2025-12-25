FROM python:3.13

WORKDIR /app
COPY requirements.txt /app/requirements.txt
RUN python3 -m pip install -r requirements.txt

COPY busserver_gw /app/busserver_gw
EXPOSE 4444/udp

ENTRYPOINT ["python3","-m","busserver_gw"]
