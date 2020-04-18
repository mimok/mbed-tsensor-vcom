/*
 * Copyright (c) 2020, Michael Grand
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "se050.h"

typedef enum {
	WAIT_FOR_CARD = 0x00,
	APDU_DATA = 0x01,
	CLOSE_CONN = 0x03
} cnx_type_t;

typedef struct {
	cnx_type_t packetType;
	uint16_t len;
	uint8_t *payload;
} packet_t;

#define BUFF_SZ 900

uint16_t send(UARTSerial *pc, packet_t *p)
{
	uint8_t header[4];
	header[0] = p->packetType;
	header[1] = 0x00;
	header[2] = (p->len >> 8) & 0x00FF;
	header[3] = p->len & 0x00FF;
	pc->write(&header[0], sizeof(header));
	pc->write(&p->payload[0], p->len);
    return MBED_SUCCESS;
}

void readUntil(UARTSerial *pc, uint8_t *buf, size_t sz)
{
	for(size_t i = 0; i<sz;) {
		i += pc->read(&buf[i], sz-i);
	}
}

uint16_t receive(UARTSerial *pc, packet_t *p)
{
	uint8_t header[4];

	readUntil(pc, &header[0], sizeof(header));
	p->packetType = (cnx_type_t) header[0];
	switch(p->packetType)
	{
	case WAIT_FOR_CARD:
	case APDU_DATA:
	case CLOSE_CONN:
		break;
	default:
		error("Unknown packet type");
	}

	p->len = (header[2]<< 8) | header[3];
	if(p->len > BUFF_SZ)
	 error("PAcket too large");

	readUntil(pc, &p->payload[0], p->len);
	return MBED_SUCCESS;
}

uint16_t process(apdu_ctx_t *ctx, packet_t *pin, packet_t *pout)
{
	switch(pin->packetType) {
	case WAIT_FOR_CARD:
		se050_initApduCtx(ctx);
		//se050_powerOn();
		if(se050_connect(ctx) == APDU_OK) {
			pout->packetType = WAIT_FOR_CARD;
			pout->len = ctx->atrLen;
			memcpy(pout->payload, ctx->atr, ctx->atrLen);
		} else {
			se050_powerOff();
			pout->packetType = WAIT_FOR_CARD;
			pout->len = 2;
			pout->payload[0] = 0x69;
			pout->payload[1] = 0x82;
		}
		break;
	case APDU_DATA:
		if(pin->payload[0] == 0xFF) {
			uint16_t timer_us;
			timer_us = (pin->payload[2] << 8) | pin->payload[3];
			thread_sleep_for(timer_us/1000);
		} else {
			ESESTATUS status = ESESTATUS_OK;
			memcpy(ctx->in.p_data, &pin->payload[0], pin->len);
			ctx->in.len = pin->len;
			ctx->out.len = BUFF_SZ; //set expected length to maximum as actual length is not known
			status = phNxpEse_Transceive(&ctx->in, &ctx->out);
			if (status == ESESTATUS_OK) {
				ctx->sw = ctx->out.p_data[ctx->out.len - 2] << 8
						| ctx->out.p_data[ctx->out.len - 1];
				ctx->out.len -= 2;
			} else {
				ctx->out.len = 0;
				ctx->sw = 0x6982;
			}
			memcpy(&pout->payload[0], &ctx->out.p_data[0], ctx->out.len);
			pout->packetType = pin->packetType;
			pout->len = ctx->out.len;
			pout->payload[pout->len++] = (ctx->sw >> 8) & 0xFF;
			pout->payload[pout->len++] = ctx->sw & 0xFF;
		}
		break;
	case CLOSE_CONN:
		if(APDU_OK == se050_disconnect(ctx)) { //does not actually close the connection as there is no such a functionality
			pout->packetType = CLOSE_CONN;
			pout->len = 2;
			pout->payload[0] = 0x90;
			pout->payload[1] = 0x00;
		} else {
			pout->packetType = CLOSE_CONN;
			pout->len = 2;
			pout->payload[0] = 0x69;
			pout->payload[1] = 0x82;
		}
		//se050_powerOff();
		break;
	default:
		error("Unknown packet type");
	}
	return MBED_SUCCESS;
}

int main(void)
{
	UARTSerial pc(USBTX, USBRX, 115200);
	DigitalOut ledin(PB_6, 0);
	DigitalOut ledout(PB_5, 0);
	uint8_t buffer[BUFF_SZ];
	apdu_ctx_t se050_ctx;

	packet_t pin = {
			.packetType = WAIT_FOR_CARD,
			.len = 0,
			.payload = &buffer[0]
	};

	packet_t pout = {
			.packetType = WAIT_FOR_CARD,
			.len = 0,
			.payload = &buffer[0]
	};
	se050_powerOn();

	while(1) {
		ledin = 1;
		receive(&pc, &pin);
		ledin = 0;
		pc.sync();
		process(&se050_ctx, &pin, &pout);
		ledout = 1;
		send(&pc, &pout);
		ledout = 0;
		pc.sync();

	}
}
