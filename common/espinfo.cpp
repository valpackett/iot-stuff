#include <espinfo.hpp>

void handle_espinfo(CoapReqCtx &ctx, char **captures, size_t ncaptures) {
	if (ctx.req->getCode() != CoapPDU::COAP_GET) return;
	ctx.resp->setCode(CoapPDU::COAP_CONTENT);
	ctx.resp->setContentFormat(CoapPDU::COAP_CONTENT_FORMAT_APP_JSON);
	StaticJsonBuffer<256> jbuf;
	JsonObject& json = jbuf.createObject();
	json["rtc_seconds"] = RTC.getRtcSeconds();
	auto freq = System.getCpuFrequency();
	if (freq == eCF_80MHz)
		json["cpu_freq"] = 80;
	else if (freq == eCF_160MHz)
		json["cpu_freq"] = 160;
	else
		json["cpu_freq"] = -1;
	char payload[256] = { 0 };
	json.printTo(payload, sizeof(payload));
	ctx.resp->setPayload((uint8_t*)&payload, strlen(payload));
}
