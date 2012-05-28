/*
* Copyright (C) Bosch Sensortec GmbH 2011
* Copyright (C) 2008 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*	   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <linux/input.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_SENSOR 1
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define MAXLINE 80
#define SERV_PORT 8888

struct sensors_poll_context_t {
	struct sensors_poll_device_t device; 
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
};

static int poll__close(struct hw_device_t *dev)
{
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	if (ctx) {
		delete ctx;
	}

	return 0;
}

static int poll__activate(struct sensors_poll_device_t *device,
        int handle, int enabled) {
	return 0;
}



static int poll__setDelay(struct sensors_poll_device_t *device,
        int handle, int64_t ns) {
	return 0;
}

static int poll__poll(struct sensors_poll_device_t *device,
        sensors_event_t* data, int count) {
	sensors_poll_context_t *dev = (sensors_poll_context_t *)device;
	if(DEBUG_SENSOR)
		LOGD("poll__poll");
	if (dev->sockfd < 0)
		return 0;

	int n;
	socklen_t len;
	char mesg[MAXLINE];

	len = sizeof(dev->cliaddr);
	/* waiting for receive data */
	n = recvfrom(dev->sockfd, mesg, MAXLINE, 0,  (struct sockaddr *)(&(dev->cliaddr)), &len);
	LOGD("n=%d,len=%d, mesg=%s", n, len,mesg);

	char *token;
	char *running = mesg;
	int i = 0;
	while((token = strsep(&running, ","))!=NULL){
		if(i == 0)
			data->acceleration.x = atof(token);
		else if(i == 1)
			data->acceleration.y = atof(token);
		else if(i == 2)
			data->acceleration.z = atof(token);
		else if(i == 3)
			data->timestamp = atoi(token);
		else
			break;

		i++;
	}

	data->sensor = 0;
	data->type = SENSOR_TYPE_ACCELEROMETER;
	data->acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
			
		
#ifdef DEBUG_SENSOR
	LOGD("Sensor data: t x,y,x: %f %f, %f, %f\n",
			data->timestamp / 1000000000.0,
					data->acceleration.x,
					data->acceleration.y,
					data->acceleration.z);
#endif
			
	return 1;
}

static const struct sensor_t sSensorList[] = {

        { 	"BMA250 3-axis Accelerometer",
                "Bosch",
                1, 0,
                SENSOR_TYPE_ACCELEROMETER, 
		4.0f*9.81f, 
		(4.0f*9.81f)/1024.0f, 
		0.2f, 
		0, 
		{ } 
	},

};

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static int sensors__get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list)
{
	*list = sSensorList;

	return ARRAY_SIZE(sSensorList);
}

static struct hw_module_methods_t sensors_module_methods = {
	open : open_sensors
};

extern "C" const struct sensors_module_t HAL_MODULE_INFO_SYM = {
	common :{
		tag : HARDWARE_MODULE_TAG,
		version_major : 1,
		version_minor : 0,
		id : SENSORS_HARDWARE_MODULE_ID,
		name : "Bosch sensor module",
		author : "Bosch Sensortec",
		methods : &sensors_module_methods,
		dso : NULL,
		reserved : {},
	},

	get_sensors_list : sensors__get_sensors_list
};

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{ 
	int status = -EINVAL;

	sensors_poll_context_t *dev = new sensors_poll_context_t();
	memset(&dev->device, 0, sizeof(sensors_poll_device_t));

	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version  = 0;
	dev->device.common.module   = const_cast<hw_module_t*>(module);
	dev->device.common.close    = poll__close;
	dev->device.activate        = poll__activate;
	dev->device.setDelay        = poll__setDelay;
	dev->device.poll            = poll__poll;

	*device = &dev->device.common;


	dev->sockfd = socket(AF_INET, SOCK_DGRAM, 0); /* create a socket */

	/* init servaddr */
	bzero(&dev->servaddr, sizeof(dev->servaddr));
	dev->servaddr.sin_family = AF_INET;
	dev->servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	dev->servaddr.sin_port = htons(SERV_PORT);

	/* bind address and port to socket */
	if(bind(dev->sockfd, (struct sockaddr *)&dev->servaddr, sizeof(dev->servaddr)) == -1){
		LOGD("bind error");
		//perror("bind error");
	}

	status = 0;

	if(DEBUG_SENSOR)
		LOGD("open_sensors");
	return status;
}
