#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <cantcoap.h>
#include <routing.hpp>

class CoapReqCtx {
	public:
		CoapPDU *req;
		CoapPDU *resp;
		CoapReqCtx(CoapPDU *req, CoapPDU *resp) {
			this->req = req;
			this->resp = resp;
		}
};

static CaptureRouteNode<CoapReqCtx> cap;

class CoapServer {
	private:
		CoapPDU respPDU;

	public:
		CoapRouter<CoapReqCtx> routes;

		void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort);
};
