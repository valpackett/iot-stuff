#pragma once

// This is kinda awful...

#include <routing.hpp>
#include <cantcoap.h>
#define SENT_MSG_IDS 8

enum ObserveCommand {
	OBS_REGISTER = 0,
	OBS_DEREGISTER = 1
};

struct Observer {
	bool active;
	int16_t fails;
	uint32_t lastMsgSeqN;
	int tokenLength;
	uint8_t token[8];
	IPAddress ip;
	uint16_t port;
	// Used as a circular buffer:
	uint16_t sentMsgIDs[SENT_MSG_IDS];
	size_t sentMsgIDCur;
};

template <UdpConnection& connection, Handler<CoapReqCtx> actualHandler,
          const size_t maxObservers = 16, const size_t maxFails = 8>
class Observable {
	public:
		static void handler(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
			actualHandler(ctx, captures, ncaptures);
			int numOptions = ctx.req->getNumOptions();
			CoapPDU::CoapOption* opt = ctx.req->getOptions();
			for (size_t i = 0; i < numOptions; i++) {
				if ((opt + i)->optionNumber == CoapPDU::COAP_OPTION_OBSERVE) {
					Observer* observer = findObserverOrEmpty(ctx);
					// Apparently clients don't actually set 0 to register and just send an empty option
					uint8_t optVal = (opt + i)->optionValueLength > 0 ? *((opt + i)->optionValuePointer) : OBS_REGISTER;
					if (optVal == OBS_REGISTER) {
						if (observer == nullptr) {
							debugf("No more slots for observers");
							return;
						}
						observer->ip = ctx.remoteIP;
						observer->port = ctx.remotePort;
						observer->tokenLength = ctx.req->getTokenLength();
						// cantcoap validates token lengths already
						memcpy(&(observer->token), ctx.req->getTokenPointer(), ctx.req->getTokenLength());
						observer->lastMsgSeqN = 0;
						observer->fails = 0;
						observer->sentMsgIDCur = 0;
						observer->active = true;
						ctx.resp->addOption(CoapPDU::COAP_OPTION_OBSERVE, 3, (uint8_t*)(&observer->lastMsgSeqN) + 1);
						debugf("Registered observer");
					} else if (optVal == OBS_DEREGISTER) {
						if (observer == nullptr) {
							debugf("Trying to deregister without registration");
							return;
						}
						observer->active = false;
						debugf("Deregistered observer %s:%d | seq id %d", observer->ip.toString().c_str(), observer->port);
					} else {
						debugf("Unknown observe option value: %d", optVal);
					}
				}
			}
		}

		static void notify() {
			CoapPDU reqPDU;
			CoapPDU respPDU;
			for (size_t i = 0; i < maxObservers; i++) {
				if (!obs[i].active) continue;
				reqPDU.reset();
				reqPDU.setVersion(1);
				reqPDU.setMessageID(RTC.getRtcSeconds() + 123 + obs[i].lastMsgSeqN);
				reqPDU.setToken(obs[i].token, obs[i].tokenLength);
				reqPDU.setCode(CoapPDU::COAP_GET);
				uint16_t respID = RTC.getRtcSeconds() * 2 + 321 + obs[i].lastMsgSeqN;
				respPDU.reset();
				respPDU.setVersion(1);
				respPDU.setMessageID(respID);
				respPDU.setToken(obs[i].token, obs[i].tokenLength);
				respPDU.setType(CoapPDU::COAP_CONFIRMABLE);
				respPDU.addOption(CoapPDU::COAP_OPTION_OBSERVE, 3, (uint8_t*)(&obs[i].lastMsgSeqN) + 1);
				CoapReqCtx ctx(&reqPDU, &respPDU, obs[i].ip, obs[i].port);
				actualHandler(ctx, nullptr, 0); // TODO support captures maybe?
				debugf("Notifying %s:%d | seq id %d", obs[i].ip.toString().c_str(), obs[i].port, obs[i].lastMsgSeqN);
				connection.sendTo(obs[i].ip, obs[i].port, (const char*)respPDU.getPDUPointer(), respPDU.getPDULength());
				obs[i].lastMsgSeqN++;
				obs[i].fails++; // Every sent msg is a potential fail. This gets decremented on ACK.
				obs[i].sentMsgIDs[obs[i].sentMsgIDCur % SENT_MSG_IDS] = respID;
				obs[i].sentMsgIDCur = (obs[i].sentMsgIDCur + 1) % SENT_MSG_IDS;
				if (obs[i].fails >= maxFails)
					obs[i].active = false;
			}
		}

		static void handleAckOrRst(CoapReqCtx& ctx) {
			// This is horrible.
			for (size_t i = 0; i < maxObservers; i++) {
				if (!(obs[i].ip == ctx.remoteIP && obs[i].port == ctx.remotePort)) continue;
				if (ctx.req->getTokenLength() >= 1) {
					if (obs[i].tokenLength != ctx.req->getTokenLength()
							|| memcmp(&obs[i].token, ctx.req->getTokenPointer(), obs[i].tokenLength) != 0) continue;
				} else {
					// Why. Why does CoAP use message IDs for matching too, not just tokens.
					bool found = false;
					for (size_t j = 0; j < SENT_MSG_IDS; j++)
						if (obs[i].sentMsgIDs[j] == ctx.req->getMessageID()) found = true;
					if (!found) continue;
				}
				// finally found the damn observer
				if (ctx.req->getType() == CoapPDU::COAP_ACKNOWLEDGEMENT) {
					debugf("Observer ACK for %s:%d", obs[i].ip.toString().c_str(), obs[i].port);
					obs[i].fails--;
				} else if (ctx.req->getType() == CoapPDU::COAP_RESET) {
					debugf("Observer RST for %s:%d", obs[i].ip.toString().c_str(), obs[i].port);
					obs[i].active = false;
				}
			}
		}

	private:
		Observable() {}
		static Observer obs[maxObservers];
		static size_t obsLen;

		static Observer* findObserverOrEmpty(CoapReqCtx &ctx) {
			for (size_t i = 0; i < maxObservers; i++)
				if (obs[i].ip == ctx.remoteIP && obs[i].port == ctx.remotePort
						&& obs[i].tokenLength == ctx.req->getTokenLength()
						&& memcmp(&obs[i].token, ctx.req->getTokenPointer(), obs[i].tokenLength) == 0) return &obs[i];
			for (size_t i = 0; i < maxObservers; i++)
				if (!obs[i].active) return &obs[i];
			return nullptr;
		}
};

template <UdpConnection& connection, Handler<CoapReqCtx> actualHandler,
          const size_t maxObservers, const size_t maxFails>
Observer Observable<connection, actualHandler, maxObservers, maxFails>::obs[maxObservers] = { 0 };

template <UdpConnection& connection, Handler<CoapReqCtx> actualHandler,
          const size_t maxObservers, const size_t maxFails>
size_t Observable<connection, actualHandler, maxObservers, maxFails>::obsLen = 0;
