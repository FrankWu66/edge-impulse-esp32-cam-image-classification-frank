import paho.mqtt.client as mqtt
import cv2
import datetime 
import time

MqttBroker="127.0.0.1"
MqttPort=1883
CloudTopic="frank/Clould_to_Edge"
EdgeTopic="frank/Edge_to_Cloud"
ISOTIMEFORMAT = '%H:%M:%S.%f'

index = 0
time_start = 0
time_end = 0

#Callback for connect.
def on_connect(client, userdata, flags, reason_code, properties):
    print("Cloud 連線結果：" + str(reason_code) + "\n")
    #訂閱Edge傳來的TOPIC
    client.subscribe(EdgeTopic)

#callback for receive subscribe update.
def on_message(client, userdata, message):
    global index
    global time_start
    global time_end
    index+=1
    filename = 's_%d.jpg' % index
    time_start = time.time()
    print('received from Edge: %s' % (datetime.datetime.now().strftime(ISOTIMEFORMAT)))
    f = open(filename,'wb+')
    f.write(message.payload)
    f.close()

    img=cv2.imread(filename)
    img=cv2.resize(img, (640, 480))
    cv2.imshow('show in clould', img)
    key=cv2.waitKey(1)  # wait 1ms for keyboard...
    #cv2.destroyWindow('image')

    payload = 'C2E_' + str(index) + "_" + datetime.datetime.now().strftime(ISOTIMEFORMAT)
    client.publish(CloudTopic, payload)
    time_end = time.time()
    time_interval = time_end - time_start
    print ("finish send message:" + payload)
    print ('spend time (in cloud): %.6f' % time_interval)
    print (" ")

'''
#callback for receive subscribe update.
def on_message(client, userdata, message):
    print('received from Edge: [%s]: %s' % (message.payload, datetime.datetime.now().strftime(ISOTIMEFORMAT)))
    payload = str(message.payload) + datetime.datetime.now().strftime(ISOTIMEFORMAT)
    client.publish(CloudTopic, payload)
    print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    print ("\n")
'''
 
#setting MQTT connect   
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.connect(MqttBroker, MqttPort, 60)
mqttc.loop_forever()