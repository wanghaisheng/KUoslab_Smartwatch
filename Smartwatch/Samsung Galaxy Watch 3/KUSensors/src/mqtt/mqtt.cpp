/*
 * mqtt.c
 *
 *  Created on: Jan 18, 2021
 *      Author: battl
 */

#include <sensors.h>
//#include <stdlib.h>
#include "mqtt.h"
#include "thpool.h"
#include <system_info.h>
#include <stdio.h>
#include <string.h>

#include "restclient/restclient.h"
#include "json/json.h"

#define THREAD_NUM	4

MQTTClient client;
MQTTClient_deliveryToken token;
threadpool thpool;

int mqttInit() {
	Json::Reader reader;
	Json::Value resJson;
	std::string clientID = (CLIENTID + std::to_string(time(NULL)));

	// 1. Get MQTT connection information(broker address, port, etc.) through REST API
	RestClient::Response r = RestClient::get(REST_API_ADDR);
	std::string res = r.body;
	dlog_print(DLOG_INFO, LOG_TAG, "r.body, %s", r.body.c_str());

	if (r.code != 200) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Can't connect to REST API Server, %d", r.code);
		// TODO Alert to users through a Tizen Pop-up message
	}
	else {
		// 2. Parse the HTTP Response & Connect to MQTT Broker
		reader.parse(res, resJson);
	}

	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int rc;
	dlog_print(DLOG_INFO, LOG_TAG, "MQTT_ADDRESS: %s, CID: %s", r.code == 200 ? resJson["uri_with_port"].asCString() : MQTT_ADDRESS, clientID.c_str());
	if ((rc = MQTTClient_create(&client, (r.code == 200 ? resJson["uri_with_port"].asCString() : MQTT_ADDRESS),
			clientID.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Can't create MQTTClient object, %d", rc);
		// TODO Alert to users through a Tizen Pop-up message
	}

	conn_opts.keepAliveInterval = 3600;
	conn_opts.cleansession = 1;

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Client is not connected with MQTT Broker, %d", rc);
		// TODO Alert to users through a Tizen Pop-up message
		//exit(-1);
	}
	else {
		dlog_print(DLOG_INFO, LOG_TAG, "MQTT Connected");
	}

	thpool = thpool_init(THREAD_NUM);
	return rc;
}

void _mqttPublish(void * msg) {
	char * tizenId;
	int ret;

	std::string str = (char *) msg;
	ret = system_info_get_platform_string("http://tizen.org/system/tizenid", &tizenId);
	if (ret != SYSTEM_INFO_ERROR_NONE) {
		/* Error handling */
		return;
	}

	char * topicName = tizenId;
	int rc;
	MQTTClient_message pubMsg = MQTTClient_message_initializer;
	pubMsg.payload = msg;
	pubMsg.payloadlen = str.length();
	pubMsg.qos = QOS;
	pubMsg.retained = 0;

	MQTTClient_publishMessage(client, topicName, &pubMsg, &token);
	rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
	free(tizenId); /* Release after use */
}

void mqttPublish(void * msg) {
	thpool_add_work(thpool, _mqttPublish, msg);
}

int mqttSubscribe() {
	//TODO - implements mqtt subscribing codes
	return 0;
}

void mqttExit() {
	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);
}
