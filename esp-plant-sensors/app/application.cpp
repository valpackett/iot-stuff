#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <esphandler.hpp>
#include <wifi.h>

// SN74HC595N 8-bit shift register is used to turn on one of the
// soil moisture sensors at a time (so basically just for I/O expansion)
// The ADC is used to read from the sensors

#define CLOCK_PIN 12
#define DATA_PIN  16
#define LATCH_PIN 14

CoapServer srv;

void shift_register_set(int bits) {
	digitalWrite(LATCH_PIN, LOW);
	shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, bits);
	digitalWrite(LATCH_PIN, HIGH);
}

void handle_not_found(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	ctx.resp->setCode(CoapPDU::COAP_NOT_FOUND);
	ctx.resp->setPayload((uint8_t*)"404", 3);
}

void handle_plant(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	if (ctx.req->getCode() != CoapPDU::COAP_GET) return;
	if (ncaptures != 1) return handle_not_found(ctx, captures, ncaptures);
	long int sensor_num = strtol(*captures, nullptr, 10);
	if (sensor_num < 0 || sensor_num > 7) return handle_not_found(ctx, captures, ncaptures);
	ctx.resp->setCode(CoapPDU::COAP_CONTENT);
	ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_JSON);
	StaticJsonBuffer<64> jbuf;
	JsonObject& json = jbuf.createObject();
	shift_register_set(1 << sensor_num);
	delay(750); // Need some time to stabilize... I've seen 1 second in examples
	json["moisture"] = system_adc_read();
	delay(50);
	shift_register_set(0);
	char payload[64] = { 0 };
	json.printTo(payload, sizeof(payload));
	ctx.resp->setPayload((uint8_t*)&payload, strlen(payload));
}

void handle_core(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	if (ctx.req->getCode() == CoapPDU::COAP_GET) {
		constexpr char *payload = (char*)(
				 "</plant/0>;if=\"sensor\";rt=\"moisture\""
				",</plant/1>;if=\"sensor\";rt=\"moisture\""
				",</plant/2>;if=\"sensor\";rt=\"moisture\""
				",</plant/3>;if=\"sensor\";rt=\"moisture\""
				",</plant/4>;if=\"sensor\";rt=\"moisture\""
				",</plant/5>;if=\"sensor\";rt=\"moisture\""
				",</plant/6>;if=\"sensor\";rt=\"moisture\""
				",</plant/7>;if=\"sensor\";rt=\"moisture\""
		);
		constexpr size_t payload_len = strlen(payload);
		ctx.resp->setCode(CoapPDU::COAP_CONTENT);
		ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_LINK);
		ctx.resp->setPayload((uint8_t*)payload, payload_len);
	}
}


void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort);

UdpConnection udp(on_receive);

void on_receive(UdpConnection& connection, char *data, int size, IPAddress remoteIP, uint16_t remotePort) {
	srv.on_receive(udp, data, size, remoteIP, remotePort);
}

void on_connected() {
	srv.routes.not_found = handle_not_found;
	srv.routes / ".well-known" / "core" = handle_core;
	srv.routes / "plant" / cap = handle_plant;
	srv.routes.debugPrint();
	udp.listen(5683);
	Serial.println("Server started");
}


void init() {
	pinMode(CLOCK_PIN, OUTPUT);
	pinMode(DATA_PIN, OUTPUT);
	pinMode(LATCH_PIN, OUTPUT);
	shift_register_set(0);
	WifiAccessPoint.enable(false, false);
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);
	delay(1200);
	WifiStation.enable(true, false);
	WifiStation.config(WIFI_SSID, WIFI_PASS);
	WifiStation.waitConnection(on_connected);
}
