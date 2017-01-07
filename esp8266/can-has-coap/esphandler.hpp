#pragma once
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <cantcoap.h>
#include <routing.hpp>

class CoapReqCtx {
	public:
		CoapPDU *req;
		CoapPDU *resp;
		IPAddress remoteIP;
		uint16_t remotePort;
		CoapReqCtx(CoapPDU *req, CoapPDU *resp, IPAddress remoteIP, uint16_t remotePort)
			: req(req), resp(resp), remoteIP(remoteIP), remotePort(remotePort) {}
};

static CaptureRouteNode<CoapReqCtx> cap;

using SubscriptionAckRstHandler = void (*)(CoapReqCtx &request);

class CoapServer {
	private:
		CoapPDU respPDU;

	public:
		CoapRouter<CoapReqCtx> routes;
		SubscriptionAckRstHandler ackRstHandler;

		void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort);
};
