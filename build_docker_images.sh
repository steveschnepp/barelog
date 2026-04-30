#! /bin.sh

for i in base arm atmega328p ch32v203 esp32s3 nrf52840 riscv rp2040
do
	docker build -t barelog:$i -f Dockerfile.$i .
done
