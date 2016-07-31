#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/DHT/DHT.h>
#include <esphandler.hpp>
#include <wifi.h>

#define DHT_PIN 0 // GPIO0

DHT dht(DHT_PIN, DHT22);
CoapServer srv;

void handle_dht(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	char payload[128] = { 0 };
	if (ctx.req->getCode() == CoapPDU::COAP_GET) {
		TempAndHumidity th;
		if (dht.readTempAndHumidity(th)) {
			ctx.resp->setCode(CoapPDU::COAP_CONTENT);
			ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_JSON);
			StaticJsonBuffer<128> jbuf;
			JsonObject& json = jbuf.createObject();
			json["temperature-c"] = th.temp;
			json["humidity-percent"] = th.humid;
			json.printTo(payload, sizeof(payload));
			ctx.resp->setPayload((uint8_t*)&payload, strlen(payload));
		} else {
			ctx.resp->setCode(CoapPDU::COAP_INTERNAL_SERVER_ERROR);
			debugf("DHT error %d", (int)dht.getLastError());
		}
	}
}

void handle_core(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	if (ctx.req->getCode() == CoapPDU::COAP_GET) {
		char *payload = (char*)"</sensors/temp>;if=\"sensor\";rt=\"temperature-c humidity-percent\"";
		ctx.resp->setCode(CoapPDU::COAP_CONTENT);
		ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_LINK);
		ctx.resp->setPayload((uint8_t*)payload, 63);
	}
}

void handle_not_found(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	ctx.resp->setCode(CoapPDU::COAP_NOT_FOUND);
	ctx.resp->setPayload((uint8_t*)"404", 3);
}


void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort);

UdpConnection udp(on_receive);

void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort) {
	srv.on_receive(udp, data, size, remoteIP, remotePort);
}

void on_connected() {
	srv.routes.not_found = handle_not_found;
	srv.routes / ".well-known" / "core" = handle_core;
	srv.routes / "sensors" / "temp" = handle_dht;
	srv.routes.debugPrint();
	udp.listen(5683);
	Serial.println("Server started");
}


void init() {
	WifiAccessPoint.enable(false, false);
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);
	delay(1200);
	dht.begin();
	WifiStation.enable(true, false);
	WifiStation.config(WIFI_SSID, WIFI_PASS);
	WifiStation.waitConnection(on_connected);
}

