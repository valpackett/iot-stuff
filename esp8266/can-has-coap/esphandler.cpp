#include <esphandler.hpp>

#define URI_LEN 128

static char uriBuffer[URI_LEN];

void CoapServer::on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort) {
	debugf("UDP recv from %s:%d | %d bytes", remoteIP.toString().c_str(), remotePort, size);
	CoapPDU *reqPDU = new CoapPDU((uint8_t*)data, size);
	if (reqPDU->validate()) {
		int recvURILen = 0;
		if (reqPDU->getURI(uriBuffer, URI_LEN, &recvURILen) != 0) {
			debugf("Error retrieving URI");
		}
		CoapReqCtx ctx(reqPDU, &respPDU, remoteIP, remotePort);
		if (recvURILen == 0) {
			debugf("No URI associated with this CoAP PDU");
			if (ackRstHandler != nullptr) ackRstHandler(ctx);
			delete reqPDU;
			return;
		} else {
			debugf("Valid request for %s", uriBuffer);
			respPDU.reset();
			respPDU.setVersion(1);
			respPDU.setMessageID(reqPDU->getMessageID());
			respPDU.setToken(reqPDU->getTokenPointer(), reqPDU->getTokenLength());
			switch (reqPDU->getType()) {
				case CoapPDU::COAP_CONFIRMABLE:
					respPDU.setType(CoapPDU::COAP_ACKNOWLEDGEMENT);
					break;
				case CoapPDU::COAP_NON_CONFIRMABLE:
					respPDU.setType(CoapPDU::COAP_ACKNOWLEDGEMENT);
					break;
				case CoapPDU::COAP_ACKNOWLEDGEMENT:
					break;
				case CoapPDU::COAP_RESET:
					break;
				default:
					break;
			};
			this->routes.handle(ctx, uriBuffer + 1);
		}
		connection.sendTo(remoteIP, remotePort, (const char*)respPDU.getPDUPointer(), respPDU.getPDULength());
	} else {
		debugf("Invalid packet");
	}
	delete reqPDU;
}
