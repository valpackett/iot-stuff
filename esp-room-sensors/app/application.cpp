#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <SmingCore/Wire.h>
#include <Libraries/DHT/DHT.h>
#include <Libraries/BMP180/BMP180.h>
#include <esphandler.hpp>
#include <wifi.h>

#define DHT_PIN 4   // GPIO4  // D2
#define PIR_PIN 5   // GPIO5  // D1
#define DEBUG_PINS \
	for (int i = 0; i < 16; i++) debugf("PIN %d %d", i, digitalRead(i));

DHT dht(DHT_PIN, DHT22);
BMP180 bmp;
CoapServer srv;

void handle_dht(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	char payload[128] = { 0 };
	if (ctx.req->getCode() != CoapPDU::COAP_GET) return;
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

void handle_bmp(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	char payload[128] = { 0 };
	if (ctx.req->getCode() != CoapPDU::COAP_GET) return;
	if (bmp.IsConnected) {
		ctx.resp->setCode(CoapPDU::COAP_CONTENT);
		ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_JSON);
		StaticJsonBuffer<128> jbuf;
		JsonObject& json = jbuf.createObject();
		json["pressure-pa"] = bmp.GetPressure();
		json["temperature-c"] = bmp.GetTemperature();
		json.printTo(payload, sizeof(payload));
		ctx.resp->setPayload((uint8_t*)&payload, strlen(payload));
	} else {
		ctx.resp->setCode(CoapPDU::COAP_INTERNAL_SERVER_ERROR);
		debugf("BMP not connected");
		if (bmp.EnsureConnected()) {
			bmp.SoftReset();
			bmp.Initialize();
		}
	}
}

void handle_pir(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	char payload[128] = { 0 };
	if (ctx.req->getCode() != CoapPDU::COAP_GET) return;
	ctx.resp->setCode(CoapPDU::COAP_CONTENT);
	ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_JSON);
	StaticJsonBuffer<128> jbuf;
	JsonObject& json = jbuf.createObject();
	json["presence"] = digitalRead(PIR_PIN) == HIGH;
	json.printTo(payload, sizeof(payload));
	ctx.resp->setPayload((uint8_t*)&payload, strlen(payload));
}

void handle_core(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	if (ctx.req->getCode() == CoapPDU::COAP_GET) {
		constexpr char *payload = (char*)(
				 "</sensors/dht>;if=\"sensor\";rt=\"temperature-c humidity-percent\""
				",</sensors/bmp>;if=\"sensor\";rt=\"pressure-pa temperature-c\""
				",</sensors/pir>;if=\"sensor\";rt=\"presence\""
		);
		constexpr size_t payload_len = strlen(payload);
		ctx.resp->setCode(CoapPDU::COAP_CONTENT);
		ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_LINK);
		ctx.resp->setPayload((uint8_t*)payload, payload_len);
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
	srv.routes / "sensors" / "dht" = handle_dht;
	srv.routes / "sensors" / "bmp" = handle_bmp;
	srv.routes / "sensors" / "pir" = handle_pir;
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
	Wire.begin();
	bmp = BMP180();
	if (bmp.EnsureConnected()) {
		bmp.SoftReset();
		bmp.Initialize();
	}
	WifiStation.enable(true, false);
	WifiStation.config(WIFI_SSID, WIFI_PASS);
	WifiStation.waitConnection(on_connected);
}

